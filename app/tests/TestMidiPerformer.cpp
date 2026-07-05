#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/keyboard/MidiPerformer.h"
#include "engine/keyboard/TouchKeyboard.h"

// M9a MIDI-in mapper (spec: 08-implementation-plan.md "M9 breakdown"):
// notes 24-29 -> drone voices, 36+ -> plates with per-note octave shift,
// velocity/aftertouch -> pressure, CC64 sustain, momentary vs latch modes.

using namespace s42;
using Catch::Approx;

namespace
{
struct Merged
{
    KbTouch touch;
    bool gate[6] = {};
};

Merged applied(const MidiPerformer& mp, const KbTouch& base = {})
{
    Merged m;
    m.touch = base;
    mp.apply(m.touch, m.gate);
    return m;
}
} // namespace

TEST_CASE("midi notes map to plates with octave shift, native at C4-B4", "[midi]")
{
    MidiPerformer mp;

    mp.noteOn(60, 0.8f); // C4 -> plate 0, native
    auto m = applied(mp);
    CHECK(m.touch.plate[0] == Approx(0.8f));
    CHECK((int) m.touch.plateShift[0] == 0);

    mp.noteOn(74, 0.6f); // D5 -> plate 2, +1 oct
    mp.noteOn(38, 0.6f); // D2 -> the same plate 2, -2 oct
    m = applied(mp);
    CHECK((int) m.touch.plateShift[2] == -2); // pressed after D5: last wins
    mp.noteOff(38);
    m = applied(mp);
    CHECK((int) m.touch.plateShift[2] == 1); // survivor 74 takes the plate back
    CHECK(m.touch.plate[2] == Approx(0.6f));

    mp.noteOff(74);
    mp.noteOff(60);
    m = applied(mp);
    CHECK(m.touch.plate[0] == 0.0f);
    CHECK(m.touch.plate[2] == 0.0f);
}

TEST_CASE("midi pressure: velocity floor, aftertouch rides above it", "[midi]")
{
    MidiPerformer mp;

    mp.noteOn(65, 0.005f); // pp velocity still crosses the touch threshold
    auto m = applied(mp);
    CHECK(m.touch.plate[5] == Approx(MidiPerformer::kMinPressure));
    CHECK(m.touch.plate[5] > TouchKeyboard::kTouchThreshold);

    mp.noteOff(65);
    mp.noteOn(65, 0.3f);
    mp.channelPressure(0.9f); // channel AT raises pressure while held
    m = applied(mp);
    CHECK(m.touch.plate[5] == Approx(0.9f));
    mp.channelPressure(0.0f); // ...and falls back to the velocity
    m = applied(mp);
    CHECK(m.touch.plate[5] == Approx(0.3f));

    mp.polyPressure(65, 0.7f); // poly AT targets its own note
    m = applied(mp);
    CHECK(m.touch.plate[5] == Approx(0.7f));

    // Merge with a stronger finger touch on the same plate: max wins.
    KbTouch finger;
    finger.plate[5] = 0.95f;
    KbTouch merged = finger;
    bool g[6];
    mp.apply(merged, g);
    CHECK(merged.plate[5] == Approx(0.95f));
}

TEST_CASE("C1-F1 gate drone voices momentarily, CC64 sustains", "[midi]")
{
    MidiPerformer mp;

    mp.noteOn(24, 0.8f); // voice 1
    mp.noteOn(29, 0.8f); // voice 6
    auto m = applied(mp);
    CHECK(m.gate[0]);
    CHECK(m.gate[5]);
    CHECK_FALSE(m.gate[1]);
    CHECK(m.touch.plate[0] == 0.0f); // drone keys never touch the plates

    mp.sustain(true);
    mp.noteOff(24);
    mp.noteOff(29);
    m = applied(mp);
    CHECK(m.gate[0]); // pedal holds the gates
    CHECK(m.gate[5]);

    mp.sustain(false);
    m = applied(mp);
    CHECK_FALSE(m.gate[0]);
    CHECK_FALSE(m.gate[5]);
}

TEST_CASE("latch mode counts HOLD toggles instead of gating", "[midi]")
{
    MidiPerformer mp;
    mp.setLatch(true);

    mp.noteOn(26, 0.8f); // voice 3 (Papa Srapa)
    mp.noteOff(26);
    auto m = applied(mp);
    CHECK_FALSE(m.gate[2]);
    CHECK(mp.toggles(2) == 1);
    mp.noteOn(26, 0.8f);
    CHECK(mp.toggles(2) == 2);
    CHECK(mp.toggles(0) == 0);
}

TEST_CASE("guard band and sub-drone notes are ignored", "[midi]")
{
    MidiPerformer mp;
    for (int note : { 0, 12, 23, 30, 33, 35 })
        mp.noteOn(note, 0.9f);
    auto m = applied(mp);
    for (int p = 0; p < 12; ++p)
        CHECK(m.touch.plate[p] == 0.0f);
    for (int v = 0; v < 6; ++v)
        CHECK_FALSE(m.gate[v]);
}

TEST_CASE("plateShift transposes the keyboard V/OCT by whole volts", "[midi][keyboard]")
{
    // The engine half of the octave-shift feature: the same plate, played
    // with shift -2/0/+1, must land exactly plateVolts + shift (quantised
    // semitone grid preserves octaves).
    auto run = [](int8_t shift)
    {
        TouchKeyboard kb;
        kb.prepare(48000.0);
        kb.setConfig(KbConfig {});
        KbTouch t;
        t.plate[0] = 0.7f;
        t.plateShift[0] = shift;
        float clk[64] = {}, rst[64] = {};
        float v[64], gl[64], gr[64], pr[64];
        for (int i = 0; i < 4; ++i)
            kb.process(t, clk, false, rst, v, gl, gr, pr, 64);
        return v[63];
    };

    const float native = run(0);
    CHECK(native == Approx(kKbPlateBaseVolts));
    CHECK(run(1) == Approx(native + 1.0f));
    CHECK(run(-2) == Approx(native - 2.0f));
    CHECK(run(120) == Approx(8.0f)); // absurd shift clamps at the 0-8 V DAC rail
}
