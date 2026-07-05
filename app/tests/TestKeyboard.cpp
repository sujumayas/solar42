#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/Rack.h"
#include "engine/keyboard/KbMenu.h"
#include "engine/keyboard/Quantiser.h"
#include "engine/keyboard/TouchKeyboard.h"

#include <cmath>
#include <set>
#include <vector>

using namespace s42;
using Catch::Approx;

namespace
{
constexpr double kSr = 48000.0;
constexpr float kBase = kKbPlateBaseVolts;

// Drives a TouchKeyboard for `samples` and collects the four outputs.
// clockEvery > 0 injects a 32-sample-high external pulse train.
struct KbRig
{
    TouchKeyboard kb;
    std::vector<float> voct, gateL, gateR, press;

    KbRig() { kb.prepare(kSr); }

    void run(const KbConfig& cfg, const KbTouch& touch, int samples,
             int clockEvery = 0, int resetAt = -1)
    {
        kb.setConfig(cfg);
        const int start = (int) voct.size();
        for (int i = start; i < start + samples; i += 64)
        {
            float clk[64] = {}, rst[64] = {};
            for (int k = 0; k < 64; ++k)
            {
                if (clockEvery > 0 && (i + k) % clockEvery < 32)
                    clk[k] = 5.0f;
                if (resetAt >= 0 && i + k >= resetAt && i + k < resetAt + 32)
                    rst[k] = 5.0f;
            }
            float v[64], gl[64], gr[64], pr[64];
            kb.process(touch, clk, clockEvery > 0, rst, v, gl, gr, pr, 64);
            for (int k = 0; k < 64; ++k)
            {
                voct.push_back(v[k]);
                gateL.push_back(gl[k]);
                gateR.push_back(gr[k]);
                press.push_back(pr[k]);
            }
        }
    }

    // Distinct voct plateau values in order of appearance (ignoring glides).
    std::vector<float> plateaus(float eps = 1.0e-4f) const
    {
        std::vector<float> out;
        for (size_t i = 1; i < voct.size(); ++i)
            if (std::abs(voct[i] - voct[i - 1]) < 1.0e-6f
                && (out.empty() || std::abs(voct[i] - out.back()) > eps))
                out.push_back(voct[i]);
        return out;
    }
};

KbTouch touchOf(std::initializer_list<int> plates, float pressure = 0.6f)
{
    KbTouch t;
    for (int p : plates)
        t.plate[p] = pressure;
    return t;
}
} // namespace

// ---------------------------------------------------------------- quantiser

TEST_CASE("quantiser snaps to the scale mask around the root", "[keyboard][quantiser]")
{
    // Chromatic: exact semitones pass through.
    CHECK(kbQuantise(3.04f, 0x0FFF, 0) == Approx(3.0).margin(1e-6));
    CHECK(kbQuantise(3.30f, 0x0FFF, 0) == Approx(3.0 + 4.0 / 12.0).margin(1e-6));

    // Pentatonic major on C: D# (3 semis) ties D/E, resolves down to D (2);
    // F (5) is nearer E (4).
    const uint16_t pent = kKbScaleMasks[10];
    CHECK(kbQuantise(3.0f + 3.0f / 12.0f, pent, 0) * 12.0f == Approx(38.0).margin(1e-4));
    CHECK(kbQuantise(3.0f + 5.0f / 12.0f, pent, 0) * 12.0f == Approx(40.0).margin(1e-4));

    // Root shifts the mask: pentatonic major on D contains E (2+2).
    CHECK(kbQuantise(3.0f + 4.0f / 12.0f, pent, 2) * 12.0f == Approx(40.0).margin(1e-4));

    // All notes off = microtonal pass-through.
    CHECK(kbQuantise(3.1234f, 0, 0) == Approx(3.1234).margin(1e-6));
}

TEST_CASE("LOAD SCALE retunes the plates to walk the scale", "[keyboard][quantiser]")
{
    KbConfig c;
    kbLoadScale(c, 1); // Ionian, root C
    const int ionian[12] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19 };
    for (int p = 0; p < 12; ++p)
        CHECK(c.plateVolts[p] == Approx(kBase + ionian[p] / 12.0).margin(1e-5));

    c.rootNote = 2; // D
    kbLoadScale(c, 1);
    CHECK(c.plateVolts[0] == Approx(kBase + 2.0 / 12.0).margin(1e-5));
    CHECK(c.scaleMask == kKbScaleMasks[1]);
}

TEST_CASE("plate tuning steps: semitones quantised, 0.0025 V microtonal", "[keyboard][quantiser]")
{
    KbConfig c;
    kbTunePlate(c, 0, 2);
    CHECK(c.plateVolts[0] == Approx(kBase + 2.0 / 12.0).margin(1e-5));

    c.scaleMask = 0; // microtonal keyboard
    kbTunePlate(c, 0, 3);
    CHECK(c.plateVolts[0] == Approx(kBase + 2.0 / 12.0 + 3 * 0.0025).margin(1e-6));
}

// ---------------------------------------------------------------- keyboard mode

TEST_CASE("plates gate and set V/oct with last-note priority", "[keyboard]")
{
    KbRig rig;
    KbConfig c;

    rig.run(c, KbTouch {}, 256);
    CHECK(rig.gateL.back() == 0.0f);
    CHECK(rig.voct.back() == Approx(kBase).margin(1e-4));

    rig.run(c, touchOf({ 2 }), 256);
    CHECK(rig.gateL.back() == TouchKeyboard::kGateVolts);
    CHECK(rig.voct.back() == Approx(kBase + 2.0 / 12.0).margin(1e-4));

    rig.run(c, touchOf({ 2, 7 }), 256); // 7 pressed later -> priority
    CHECK(rig.voct.back() == Approx(kBase + 7.0 / 12.0).margin(1e-4));

    rig.run(c, touchOf({ 2 }), 256); // release 7 -> back to 2
    CHECK(rig.voct.back() == Approx(kBase + 2.0 / 12.0).margin(1e-4));

    rig.run(c, KbTouch {}, 256); // release everything: gate low, V/oct holds
    CHECK(rig.gateL.back() == 0.0f);
    CHECK(rig.voct.back() == Approx(kBase + 2.0 / 12.0).margin(1e-4));

    // Single behaviour: GATE R mirrors GATE L.
    for (size_t i = 0; i < rig.gateL.size(); ++i)
        REQUIRE(rig.gateR[i] == rig.gateL[i]);
}

TEST_CASE("portamento glides between plates; legato needs two fingers", "[keyboard]")
{
    KbConfig c;
    c.portamento = 64; // ~0.16 s glide

    KbRig rig;
    rig.run(c, touchOf({ 0 }), 512);
    rig.run(c, touchOf({ 11 }), 480); // jump nearly an octave
    const float target = kBase + 11.0f / 12.0f;
    // Mid-glide after 10 ms: clearly moving, clearly not arrived.
    CHECK(rig.voct.back() > kBase + 0.1f);
    CHECK(rig.voct.back() < target - 0.1f);
    rig.run(c, touchOf({ 11 }), 48000);
    CHECK(rig.voct.back() == Approx(target).margin(1e-3));

    // Legato: a lone new touch jumps instantly...
    c.legato = true;
    KbRig leg;
    leg.run(c, touchOf({ 0 }), 512);
    leg.run(c, touchOf({ 11 }), 128);
    CHECK(leg.voct.back() == Approx(target).margin(1e-4));
    // ...but overlapping touches glide (second finger down, ~0.1 s in).
    leg.run(c, touchOf({ 11, 0 }), 4800);
    CHECK(leg.voct.back() < target - 0.05f);
    CHECK(leg.voct.back() > kBase + 0.05f);
}

TEST_CASE("quantiser off turns the plates microtonal", "[keyboard]")
{
    KbConfig c;
    c.scaleMask = 0;
    c.plateVolts[3] = 3.3712f; // hand-tuned, off the semitone grid
    KbRig rig;
    rig.run(c, touchOf({ 3 }), 256);
    CHECK(rig.voct.back() == Approx(3.3712).margin(1e-5));
}

TEST_CASE("transpose buttons: octave shift (quantised) / offset toggle (microtonal)", "[keyboard]")
{
    KbConfig c;
    KbRig rig;
    rig.run(c, touchOf({ 0 }), 128);

    KbTouch t = touchOf({ 0 });
    t.button[1] = true; // octave up
    rig.run(c, t, 128);
    CHECK(rig.voct.back() == Approx(kBase + 1.0).margin(1e-4));
    t.button[1] = false;
    rig.run(c, t, 64);
    t.button[0] = true; // octave down (edge-triggered)
    rig.run(c, t, 128);
    CHECK(rig.voct.back() == Approx(kBase).margin(1e-4));

    // Microtonal: buttons toggle their configured offsets instead.
    KbConfig m;
    m.scaleMask = 0;
    m.offsetVolts[1] = 0.62f;
    KbRig mr;
    mr.run(m, touchOf({ 0 }), 128);
    KbTouch mt = touchOf({ 0 });
    mt.button[1] = true;
    mr.run(m, mt, 128);
    mt.button[1] = false;
    mr.run(m, mt, 128);
    CHECK(mr.voct.back() == Approx(kBase + 0.62).margin(1e-4)); // latched on
    mt.button[1] = true;
    mr.run(m, mt, 128); // second press toggles off
    CHECK(mr.voct.back() == Approx(kBase).margin(1e-4));
}

// ---------------------------------------------------------------- behaviours

TEST_CASE("twin behaviour: PRESSURE jack becomes the right side's V/oct", "[keyboard]")
{
    KbConfig c;
    c.behaviour = KbBehaviour::Twin;
    KbRig rig;

    KbTouch t;
    t.plate[2] = 0.5f; // left side, local plate 2
    t.plate[8] = 0.5f; // right side, local plate 2 (global 8)
    rig.run(c, t, 256);

    CHECK(rig.voct.back() == Approx(kBase + 2.0 / 12.0).margin(1e-4));
    CHECK(rig.press.back() == Approx(kBase + 8.0 / 12.0).margin(1e-4));
    CHECK(rig.gateL.back() == TouchKeyboard::kGateVolts);
    CHECK(rig.gateR.back() == TouchKeyboard::kGateVolts);

    // Release only the right side: GATE R drops, GATE L stays.
    t.plate[8] = 0.0f;
    rig.run(c, t, 256);
    CHECK(rig.gateL.back() == TouchKeyboard::kGateVolts);
    CHECK(rig.gateR.back() == 0.0f);
    CHECK(rig.press.back() == Approx(kBase + 8.0 / 12.0).margin(1e-4)); // holds
}

TEST_CASE("split behaviour: sides run different modes", "[keyboard]")
{
    KbConfig c;
    c.behaviour = KbBehaviour::Split;
    c.side[0].mode = KbMode::Arp; // left arpeggiates...
    c.side[1].mode = KbMode::Keyboard; // ...right plays plain keyboard

    KbRig rig;
    KbTouch t;
    t.plate[0] = t.plate[2] = 0.5f; // left chord
    t.plate[9] = 0.5f;              // right note
    rig.run(c, t, 2048, 256);

    CHECK(rig.press.back() == Approx(kBase + 9.0 / 12.0).margin(1e-4));
    // The left side stepped through both chord notes.
    std::set<int> semis;
    for (float v : rig.voct)
        semis.insert((int) std::lround((double) v * 12.0));
    CHECK(semis.count((int) std::lround(kBase * 12) + 0) == 1);
    CHECK(semis.count((int) std::lround(kBase * 12) + 2) == 1);
}

// ---------------------------------------------------------------- arpeggiator

TEST_CASE("arp orders notes by plate number and direction", "[keyboard][arp]")
{
    Arpeggiator arp;
    ArpConfig a;
    const uint16_t chord = (1u << 1) | (1u << 4) | (1u << 7);

    std::vector<int> fwd;
    for (int i = 0; i < 6; ++i)
        if (arp.tick(chord, a))
            fwd.push_back(arp.curPlate);
    CHECK(fwd == std::vector<int> { 1, 4, 7, 1, 4, 7 });

    a.direction = KbDirection::Backward;
    arp.reset();
    std::vector<int> back;
    for (int i = 0; i < 3; ++i)
        if (arp.tick(chord, a))
            back.push_back(arp.curPlate);
    CHECK(back == std::vector<int> { 7, 4, 1 });

    a.direction = KbDirection::PingPong; // endpoints not repeated
    arp.reset();
    std::vector<int> pp;
    for (int i = 0; i < 8; ++i)
        if (arp.tick(chord, a))
            pp.push_back(arp.curPlate);
    CHECK(pp == std::vector<int> { 1, 4, 7, 4, 1, 4, 7, 4 });

    a.direction = KbDirection::Random;
    arp.reset();
    for (int i = 0; i < 32; ++i)
    {
        REQUIRE(arp.tick(chord, a));
        REQUIRE((arp.curPlate == 1 || arp.curPlate == 4 || arp.curPlate == 7));
    }
}

TEST_CASE("arp variation replays transposed by the interval", "[keyboard][arp]")
{
    Arpeggiator arp;
    ArpConfig a;
    a.variation = 2;
    a.intervalSemis = 12;
    const uint16_t chord = (1u << 0) | (1u << 3);

    std::vector<int> semis;
    for (int i = 0; i < 6; ++i)
        if (arp.tick(chord, a))
            semis.push_back(arp.curTransposeSemis);
    CHECK(semis == std::vector<int> { 0, 0, 12, 12, 24, 24 });
}

TEST_CASE("arp rhythm mask mutes clock pulses", "[keyboard][arp]")
{
    Arpeggiator arp;
    ArpConfig a;
    a.rhythmLen = 4;
    a.rhythmMask = 0b0101; // steps 1 and 3 pass
    const uint16_t chord = 1u << 2;

    int fired = 0;
    for (int i = 0; i < 8; ++i)
        fired += arp.tick(chord, a) ? 1 : 0;
    CHECK(fired == 4);
}

TEST_CASE("arp follows the external clock and HOLD latches the chord", "[keyboard][arp]")
{
    KbConfig c;
    c.side[0].mode = KbMode::Arp;
    c.side[0].arp.hold = true;

    KbRig rig;
    rig.run(c, touchOf({ 0, 4 }), 1024, 256); // hold two plates, ext clock
    rig.run(c, KbTouch {}, 2048, 256);        // release: HOLD keeps arpeggiating

    // Gates keep pulsing after release...
    bool gateAfterRelease = false;
    for (size_t i = rig.gateL.size() - 1024; i < rig.gateL.size(); ++i)
        gateAfterRelease = gateAfterRelease || rig.gateL[i] > 5.0f;
    CHECK(gateAfterRelease);

    // ...alternating between exactly the two latched notes.
    std::set<int> semis;
    for (size_t i = rig.voct.size() - 1024; i < rig.voct.size(); ++i)
        semis.insert((int) std::lround((double) rig.voct[i] * 12.0));
    CHECK(semis == std::set<int> { (int) std::lround(kBase * 12),
                                   (int) std::lround(kBase * 12) + 4 });
}

TEST_CASE("RESET jack returns the arp to its first step", "[keyboard][arp]")
{
    // Deterministic check at the unit level (the jack edge calls reset()).
    Arpeggiator arp;
    ArpConfig a;
    const uint16_t chord = (1u << 0) | (1u << 5) | (1u << 9);
    (void) arp.tick(chord, a);
    (void) arp.tick(chord, a);
    arp.reset();
    REQUIRE(arp.tick(chord, a));
    CHECK(arp.curPlate == 0); // first note again

    // And through the rig: a reset pulse mid-run lands back on plate 0.
    KbConfig c;
    c.side[0].mode = KbMode::Arp;
    KbRig rig;
    rig.run(c, touchOf({ 0, 4, 7 }), 4096, 512, 1300);
    bool sawBase = false;
    for (size_t i = 0; i < rig.voct.size(); ++i)
        sawBase = sawBase || std::abs(rig.voct[i] - kBase) < 1e-4f;
    CHECK(sawBase);
}

// ---------------------------------------------------------------- sequencer

TEST_CASE("sequencer directions, incl. ping-pong endpoint repeats", "[keyboard][seq]")
{
    KbSequencer seq;
    SeqConfig s;
    s.length = 4;

    std::vector<int> fwd;
    for (int i = 0; i < 8; ++i)
        if (seq.tick(s))
            fwd.push_back(seq.curStep);
    CHECK(fwd == std::vector<int> { 0, 1, 2, 3, 0, 1, 2, 3 });

    s.direction = KbDirection::PingPong;
    seq.reset();
    std::vector<int> pp;
    for (int i = 0; i < 10; ++i)
        if (seq.tick(s))
            pp.push_back(seq.curStep);
    CHECK(pp == std::vector<int> { 0, 1, 2, 3, 3, 2, 1, 0, 0, 1 });

    s.direction = KbDirection::Backward;
    seq.reset();
    std::vector<int> back;
    for (int i = 0; i < 4; ++i)
        if (seq.tick(s))
            back.push_back(seq.curStep);
    CHECK(back == std::vector<int> { 3, 2, 1, 0 });
}

TEST_CASE("sequencer CV: continuous vs gated update, plate transpose", "[keyboard][seq]")
{
    KbConfig c;
    c.side[0].mode = KbMode::Seq;
    auto& s = c.side[0].seq;
    s.length = 4;
    s.value[0] = 0.0f;
    s.value[1] = 3.0f / 12.0f;
    s.value[2] = 7.0f / 12.0f;
    s.value[3] = 1.0f;
    s.gate[1] = false;
    s.gate[3] = false;

    // Continuous: every step's value reaches the CV output (transposed by
    // the untouched default plate value = kBase).
    KbRig cont;
    cont.run(c, KbTouch {}, 8192, 512);
    std::set<int> semis;
    for (float v : cont.voct)
        semis.insert((int) std::lround((double) v * 12.0));
    const int b = (int) std::lround(kBase * 12);
    CHECK(semis == std::set<int> { b, b + 3, b + 7, b + 12 });

    // Gated: values on gate-off steps never appear.
    s.gatedCv = true;
    KbRig gated;
    gated.run(c, KbTouch {}, 8192, 512);
    semis.clear();
    for (float v : gated.voct)
        semis.insert((int) std::lround((double) v * 12.0));
    CHECK(semis == std::set<int> { b, b + 7 });

    // Pressing a plate transposes the whole sequence.
    s.gatedCv = false;
    KbRig trans;
    trans.run(c, touchOf({ 5 }), 8192, 512);
    bool sawTransposed = false;
    for (float v : trans.voct)
        sawTransposed = sawTransposed
                        || std::abs(v - (kBase + 5.0f / 12.0f + 7.0f / 12.0f)) < 1e-4f;
    CHECK(sawTransposed);
}

TEST_CASE("key-run sequencer only advances while a plate is held", "[keyboard][seq]")
{
    KbConfig c;
    c.side[0].mode = KbMode::Seq;
    c.side[0].seq.keyRun = true;
    c.side[0].seq.length = 8;
    for (int i = 0; i < 8; ++i)
        c.side[0].seq.value[i] = (float) i / 12.0f;

    KbRig rig;
    rig.run(c, KbTouch {}, 4096, 256); // untouched: frozen
    std::set<int> semis;
    for (float v : rig.voct)
        semis.insert((int) std::lround((double) v * 12.0));
    CHECK(semis.size() == 1);

    rig.run(c, touchOf({ 0 }), 4096, 256); // held: it runs
    semis.clear();
    for (float v : rig.voct)
        semis.insert((int) std::lround((double) v * 12.0));
    CHECK(semis.size() > 3);
}

// ---------------------------------------------------------------- clock

TEST_CASE("clock auto-switches to external and BPM edits switch back", "[keyboard][clock]")
{
    KbClock clk;
    clk.prepare(kSr);

    // Internal: 2 ticks in 27000 samples at 240 BPM (period 12000 samples).
    int ticks = 0;
    for (int i = 0; i < 27000; ++i)
        ticks += clk.tick(0.0f, false, 240.0f, 0) ? 1 : 0;
    CHECK(ticks == 2);
    CHECK_FALSE(clk.external);

    // A pulse train takes over; ticks land on the edges only.
    ticks = 0;
    for (int i = 0; i < 4000; ++i)
        ticks += clk.tick(i % 1000 < 100 ? 5.0f : 0.0f, true, 240.0f, 0) ? 1 : 0;
    CHECK(clk.external);
    CHECK(ticks == 4);
    CHECK(clk.periodSamples(240.0f) == Approx(1000.0f));

    // No pulses = no ticks (hardware behaviour: the clock stalls)...
    ticks = 0;
    for (int i = 0; i < 24000; ++i)
        ticks += clk.tick(0.0f, true, 240.0f, 0) ? 1 : 0;
    CHECK(ticks == 0);

    // ...until the BPM is edited, which reverts to internal.
    ticks = 0;
    for (int i = 0; i < 27000; ++i)
        ticks += clk.tick(0.0f, true, 240.0f, 1) ? 1 : 0;
    CHECK_FALSE(clk.external);
    CHECK(ticks == 2);
}

TEST_CASE("clock scaler divides and multiplies the base clock", "[keyboard][clock]")
{
    KbClockScaler div;
    int out = 0;
    for (int i = 0; i < 8; ++i)
        out += div.tick(true, 100.0f, kKbClockUnity - 1) ? 1 : 0; // /2
    CHECK(out == 4);

    KbClockScaler mult; // x2: base tick plus one midway
    std::vector<int> at;
    for (int i = 0; i < 300; ++i)
        if (mult.tick(i % 100 == 0, 100.0f, kKbClockUnity + 1))
            at.push_back(i);
    CHECK(at == std::vector<int> { 0, 50, 100, 150, 200, 250 });
}

// ---------------------------------------------------------------- pressure

TEST_CASE("pressure output modes shape the PRESSURE jack", "[keyboard][pressure]")
{
    const float dt = (float) (1.0 / kSr);

    PressureShaper raw; // follows applied pressure instantly at rise/fall 0
    float v = 0.0f;
    for (int i = 0; i < 4; ++i)
        v = raw.process(0.5f, true, i == 0, KbPressureMode::Pressure, 0.0f, 0.0f, dt);
    CHECK(v == Approx(4.0).margin(1e-4));

    PressureShaper asr; // ~0.1 s attack to 8 V
    v = 0.0f;
    for (int i = 0; i < 2400; ++i)
        v = asr.process(0.3f, true, i == 0, KbPressureMode::Asr, 0.1f, 0.1f, dt);
    CHECK(v == Approx(4.0).margin(0.1)); // half-way after 50 ms
    for (int i = 0; i < 4800; ++i)
        v = asr.process(0.3f, true, false, KbPressureMode::Asr, 0.1f, 0.1f, dt);
    CHECK(v == Approx(8.0).margin(1e-3));
    for (int i = 0; i < 9600; ++i)
        v = asr.process(0.0f, false, false, KbPressureMode::Asr, 0.1f, 0.1f, dt);
    CHECK(v == Approx(0.0).margin(1e-3));

    PressureShaper ad; // one-shot: completes after release
    for (int i = 0; i < 240; ++i)
        v = ad.process(0.5f, true, i == 0, KbPressureMode::Ad, 0.001f, 0.01f, dt);
    bool decayed = false;
    for (int i = 0; i < 2400; ++i)
    {
        v = ad.process(0.0f, false, false, KbPressureMode::Ad, 0.001f, 0.01f, dt);
        decayed = decayed || v > 0.0f;
    }
    CHECK(decayed);
    CHECK(v == Approx(0.0).margin(1e-4));

    PressureShaper loop; // keeps cycling while touched
    int peaks = 0;
    float prev = 0.0f;
    bool wasRising = false;
    for (int i = 0; i < 48000; ++i)
    {
        v = loop.process(0.5f, true, i == 0, KbPressureMode::LoopAd, 0.02f, 0.02f, dt);
        const bool rising = v > prev;
        if (wasRising && !rising)
            ++peaks;
        wasRising = rising;
        prev = v;
    }
    CHECK(peaks >= 10);

    PressureShaper rnd; // new value per touch, held between touches
    float a = 0.0f, b = 0.0f;
    for (int i = 0; i < 8; ++i)
        a = rnd.process(0.5f, true, i == 0, KbPressureMode::Random, 0.0f, 0.0f, dt);
    for (int i = 0; i < 8; ++i)
        b = rnd.process(0.0f, false, false, KbPressureMode::Random, 0.0f, 0.0f, dt);
    CHECK(a == Approx(b)); // held after release
    for (int i = 0; i < 8; ++i)
        b = rnd.process(0.5f, true, i == 0, KbPressureMode::Random, 0.0f, 0.0f, dt);
    CHECK(a != b); // fresh sample on the next touch
}

// ---------------------------------------------------------------- presets + menu

TEST_CASE("presets A-D round-trip everything except the tempo", "[keyboard][presets]")
{
    KbPresets p;
    KbConfig c;
    c.portamento = 99;
    c.side[0].mode = KbMode::Arp;
    c.bpm = 200.0f;
    p.slot[2] = c; // save

    KbConfig live;
    live.bpm = 66.0f;
    loadPreset(live, p.slot[2]);
    CHECK(live.portamento == 99);
    CHECK(live.side[0].mode == KbMode::Arp);
    CHECK(live.bpm == Approx(66.0f)); // tempo excluded
}

TEST_CASE("encoder menu edits parameters and drives the sub-editors", "[keyboard][menu]")
{
    KbMenu menu;
    KbConfig c;
    KbPresets p;

    // Browse: Behaviour -> Mode (SIDE hidden outside split); edit to ARP.
    menu.rotate(1, c, p);
    auto d = menu.display(c);
    CHECK(std::string(d.line1) == "MODE");
    menu.click(c, p);
    menu.rotate(1, c, p);
    CHECK(c.side[0].mode == KbMode::Arp);
    menu.click(c, p);
    CHECK_FALSE(menu.display(c).editing);

    // BPM edits bump the internal-clock reselect counter.
    const uint32_t edits = c.bpmEdit;
    for (int i = 0; i < 12; ++i) // ...to CLOCK BPM (2 items before wrap)
        menu.rotate(-1, c, p);
    // Find CLOCK BPM by scanning at most one full cycle.
    for (int i = 0; i < 40 && std::string(menu.display(c).line1) != "CLOCK BPM"; ++i)
        menu.rotate(1, c, p);
    REQUIRE(std::string(menu.display(c).line1) == "CLOCK BPM");
    menu.click(c, p);
    menu.rotate(5, c, p);
    CHECK(c.bpm == Approx(125.0f));
    CHECK(c.bpmEdit > edits);
    menu.click(c, p);

    // Scale editor: toggle a note off.
    for (int i = 0; i < 40 && std::string(menu.display(c).line1) != "SCALE EDIT"; ++i)
        menu.rotate(1, c, p);
    menu.click(c, p);
    menu.rotate(1, c, p); // cursor to note 1
    menu.click(c, p);     // toggle off
    CHECK((c.scaleMask & 0b10) == 0);
    menu.longPress(c, p); // leave the editor

    // Preset page: save into B, mangle, load back.
    for (int i = 0; i < 40 && std::string(menu.display(c).line1) != "PRESETS"; ++i)
        menu.rotate(1, c, p);
    menu.click(c, p);     // -> preset select
    menu.rotate(1, c, p); // B
    menu.click(c, p);     // -> action
    menu.rotate(1, c, p); // SAVE
    menu.click(c, p);
    CHECK(p.slot[1].side[0].mode == KbMode::Arp);

    c.side[0].mode = KbMode::Seq;
    for (int i = 0; i < 40 && std::string(menu.display(c).line1) != "PRESETS"; ++i)
        menu.rotate(1, c, p);
    menu.click(c, p);
    menu.rotate(1, c, p); // B
    menu.click(c, p);     // LOAD is the default action
    menu.click(c, p);
    CHECK(c.side[0].mode == KbMode::Arp);
}

TEST_CASE("menu seq editor walks gate and value slots", "[keyboard][menu]")
{
    KbMenu menu;
    KbConfig c;
    KbPresets p;

    for (int i = 0; i < 40 && std::string(menu.display(c).line1) != "SEQ EDITOR"; ++i)
        menu.rotate(1, c, p);
    REQUIRE(std::string(menu.display(c).line1) == "SEQ EDITOR");
    menu.click(c, p); // enter editor at step 1 gate

    menu.click(c, p); // toggle gate 1 off
    CHECK_FALSE(c.side[0].seq.gate[0]);

    menu.rotate(1, c, p); // -> step 1 value slot
    menu.click(c, p);     // highlight
    menu.rotate(3, c, p); // +3 semitones (quantiser on)
    CHECK(c.side[0].seq.value[0] == Approx(3.0 / 12.0).margin(1e-5));
    menu.click(c, p); // unhighlight
    menu.longPress(c, p);
    CHECK(std::string(menu.display(c).line1) == "SEQ EDITOR");
}

// ---------------------------------------------------------------- rack wiring

TEST_CASE("keyboard jacks are live: pulser clocks the arp, gate reaches Env A", "[keyboard][rack]")
{
    Rack rack;
    rack.prepare(48000.0, 512);

    Rack::Controls c;
    c.kb.side[0].mode = KbMode::Arp;
    c.kbTouch.plate[0] = 0.7f;
    c.kbTouch.plate[7] = 0.7f;
    c.seq.pulserHz = 40.0f; // fast pulser as the arp clock
    rack.setControls(c);

    // M6 verify criterion: arp locked to a patched pulser clock.
    rack.requestPatch(Inlet::KbClockIn, Outlet::SeqClockOut);

    std::vector<float> l(48000), r(48000);
    std::set<int> semis;
    bool gateSeen = false, envGateSeen = false;
    for (int i = 0; i < 48000; i += 512)
    {
        rack.process(l.data() + i, r.data() + i, nullptr, nullptr, 512);
        const float* voct = rack.bus().outRead(Outlet::KbVoctOut);
        const float* gate = rack.bus().outRead(Outlet::KbGateLOut);
        const float* envGate = rack.bus().in(Inlet::EnvAGateIn); // normalled from GATE L
        for (int k = 0; k < VoltBus::kSubBlock; ++k)
        {
            semis.insert((int) std::lround((double) voct[k] * 12.0));
            gateSeen = gateSeen || gate[k] > 5.0f;
            envGateSeen = envGateSeen || envGate[k] > 5.0f;
        }
    }

    const int b = (int) std::lround((double) kKbPlateBaseVolts * 12.0);
    CHECK(semis.count(b) == 1);
    CHECK(semis.count(b + 7) == 1);
    CHECK(gateSeen);
    CHECK(envGateSeen); // hardware normal GATE L -> Envelope A gate
}
