// solar42n_calib — the M8 calibration audition kit.
//
// Renders one focused WAV per tunable domain of dsp/TuningConstants.h,
// through the REAL processor (so every knob->physical map, smoother and
// tolerance is the one the user's knobs hit). Each scene prints a timeline
// sheet; the listening protocol lives in 09-calibration-protocol.md.
//
// usage: solar42n_calib <outDir> [sceneName]
#include "plugin/PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

void writeWav16(const juce::File& file, const std::vector<float>& interleaved)
{
    const uint32_t frames = (uint32_t) (interleaved.size() / 2);
    const uint16_t channels = 2, bits = 16;
    const uint32_t dataBytes = frames * channels * bits / 8;

    FILE* f = std::fopen(file.getFullPathName().toRawUTF8(), "wb");
    if (f == nullptr)
    {
        std::printf("cannot open %s\n", file.getFullPathName().toRawUTF8());
        std::exit(1);
    }
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(channels);
    u32((uint32_t) kSr);
    u32((uint32_t) kSr * channels * bits / 8);
    u16((uint16_t) (channels * bits / 8));
    u16(bits);
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    for (float v : interleaved)
    {
        const auto s = (int16_t) (juce::jlimit(-1.0f, 1.0f, v) * 32767.0f);
        std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
}

// Normalized (0..1) parameter set — bools 0/1, choices index/(count-1).
void set(Solar42NProcessor& p, const char* id, float norm)
{
    if (auto* rp = p.apvts().getParameter(id))
        rp->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
    else
    {
        std::printf("calib: unknown param '%s'\n", id);
        std::exit(1);
    }
}

// Mute the whole mixer, then scenes open only what they audition.
void muteAll(Solar42NProcessor& p)
{
    for (const char* ch : { "d1", "d2", "d3", "ext", "vcoA", "vcoB", "pre", "d4", "d5", "d6" })
        set(p, ("mix." + juce::String(ch) + ".vol").toRawUTF8(), 0.0f);
    set(p, "fx.blend", 0.0f); // fully dry unless a scene wants the effector
    set(p, "filt.gain", 0.0f);
    set(p, "master.vol", 0.75f);
    set(p, "room.light", 0.0f); // sensors dark unless the scene lights them
}

// A beating DRONE 1 cluster — the house test signal (M1's voice).
void droneCluster(Solar42NProcessor& p, bool hold = true)
{
    set(p, "d1.gen1.tune", 0.55f);
    set(p, "d1.gen2.tune", 0.32f);
    set(p, "d1.gen3.tune", 0.22f);
    set(p, "d1.gen4.tune", 0.35f);
    set(p, "d1.gen3.mute", 0.0f);
    set(p, "d1.gen4.mute", 0.0f);
    set(p, "d1.gen5.mute", 1.0f);
    set(p, "d1.att", 0.3f);
    set(p, "d1.rls", 0.4f);
    set(p, "d1.hold", hold ? 1.0f : 0.0f);
    set(p, "mix.d1.vol", 0.75f);
    set(p, "mix.d1.pan", 0.5f);
}

struct Scene
{
    const char* name;
    const char* doc; // one-line "what this calibrates"
    double seconds;
    std::function<void(Solar42NProcessor&)> setup;
    std::function<void(Solar42NProcessor&, double)> automate; // called per block
    std::vector<const char*> timeline;
};

std::vector<Scene> buildScenes()
{
    std::vector<Scene> scenes;

    // ---- 1: drone AR envelope range (kDroneAtt*/kDroneRls*)
    scenes.push_back({
        "drone-env",
        "kDroneAttMin/Max + kDroneRlsMin/Max — gate cycles at knob 0 / 0.75 / 1.0",
        28.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p, false);
        },
        [](Solar42NProcessor& p, double t)
        {
            auto seg = [&](double a, double b) { return t >= a && t < b; };
            if (seg(0, 4))       { set(p, "d1.att", 0.0f);  set(p, "d1.rls", 0.0f);  set(p, "d1.gate", t < 2 ? 1.f : 0.f); }
            else if (seg(4, 14)) { set(p, "d1.att", 0.75f); set(p, "d1.rls", 0.75f); set(p, "d1.gate", t < 10 ? 1.f : 0.f); }
            else                 { set(p, "d1.att", 1.0f);  set(p, "d1.rls", 1.0f);  set(p, "d1.gate", t < 22 ? 1.f : 0.f); }
        },
        { "0-4s   knob 0: instant on/off (min times)",
          "4-14s  knob 0.75: ~1 s swell, ~2.5 s fade",
          "14-28s knob 1.0: 8 s of a 10 s swell, then the 20 s fade (truncated)" },
    });

    // ---- 2: VCO ADSR range + LOOP (kVcoAtt*/kVcoDec*/kVcoRel*)
    scenes.push_back({
        "vco-env",
        "kVcoAtt/Dec/RelMin/Max — plate-gated ADSR fast, slow, then LOOP as LFO",
        24.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            set(p, "mix.vcoA.vol", 0.8f);
            set(p, "vcoA.wave", 0.05f); // saw
            set(p, "vcoA.tune", 0.4f);
            set(p, "envA.a", 0.0f);
            set(p, "envA.d", 0.35f);
            set(p, "envA.s", 0.6f);
            set(p, "envA.r", 0.3f);
        },
        [](Solar42NProcessor& p, double t)
        {
            const double local = std::fmod(t, 2.0);
            if (t < 8.0) // fast ADSR, 0.5 s presses every 2 s
                p.kbTouch().plate[0].store(local < 0.5 ? 0.7f : 0.0f);
            else if (t < 16.0) // slow swell, one long press
            {
                set(p, "envA.a", 0.8f);
                set(p, "envA.r", 0.8f);
                p.kbTouch().plate[0].store(t < 13.0 ? 0.7f : 0.0f);
            }
            else // LOOP: env self-retriggers at min-ish times = env-as-LFO
            {
                set(p, "envA.a", 0.18f);
                set(p, "envA.d", 0.22f);
                set(p, "envA.s", 0.0f);
                set(p, "envA.loop", 1.0f);
                p.kbTouch().plate[0].store(0.0f);
            }
        },
        { "0-8s   knob-0 ADSR: percussive plate blips (min A/D/R)",
          "8-16s  knob-0.8 A/R: slow swell + long tail",
          "16-24s LOOP at short A/D: envelope free-runs as an LFO" },
    });

    // ---- 3: LFO A/B ranges (kLfoA*/kLfoB* x range switch)
    scenes.push_back({
        "lfo-ranges",
        "kLfoAMin/Max + kLfoBMin/Max — heard as filter-cutoff wobble, 4 s per step",
        32.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p);
            set(p, "filt.freqL", 0.45f);
            set(p, "filt.freqR", 0.45f);
            set(p, "filt.resL", 0.5f);
            set(p, "filt.resR", 0.5f);
            set(p, "filt.modL", 0.6f);
            set(p, "filt.modR", 0.6f);
            set(p, "lfoA.wave", 1.0f); // triangle
            set(p, "lfoB.wave", 1.0f);
            p.patchBay().connect(s42::Inlet::FiltCvLIn, s42::Outlet::LfoAOut);
        },
        [](Solar42NProcessor& p, double t)
        {
            auto at = [&](double edge) { return t >= edge && t < edge + 0.05; };
            if (t < 4)        { set(p, "lfoA.rate", 0.0f); set(p, "lfoA.range", 0.0f); }
            else if (t < 8)   { set(p, "lfoA.rate", 1.0f); }
            else if (t < 12)  { set(p, "lfoA.range", 0.5f); }
            else if (t < 16)  { set(p, "lfoA.range", 1.0f); }
            else if (at(16))  { p.patchBay().connect(s42::Inlet::FiltCvLIn, s42::Outlet::LfoBOut); set(p, "lfoB.rate", 0.0f); set(p, "lfoB.range", 0.0f); }
            else if (t < 24 && t >= 20) { set(p, "lfoB.rate", 0.5f); }
            else if (t >= 24 && t < 28) { set(p, "lfoB.rate", 1.0f); }
            else if (t >= 28) { set(p, "lfoB.range", 1.0f); }
        },
        { "0-4s   LFO A x1 knob 0 (0.02 Hz: near-static drift)",
          "4-8s   LFO A x1 knob 1 (5 Hz wobble)",
          "8-12s  LFO A x6 (30 Hz flutter)",
          "12-16s LFO A x10 (50 Hz growl)",
          "16-20s LFO B x1 knob 0 (0.2 Hz)",
          "20-24s LFO B x1 knob 0.5 (~3 Hz)",
          "24-28s LFO B x1 knob 1 (50 Hz)",
          "28-32s LFO B x10 (500 Hz — audio-rate cutoff FM)" },
    });

    // ---- 4: pulser clock range (kPulserMin/Max)
    scenes.push_back({
        "pulser",
        "kPulserMin/Max — sequencer blips on VCO A, pulser knob swept 0 -> 1",
        16.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            set(p, "mix.vcoA.vol", 0.8f);
            set(p, "vcoA.wave", 0.05f);
            set(p, "envA.a", 0.0f);
            set(p, "envA.d", 0.25f);
            set(p, "envA.s", 0.3f);
            set(p, "envA.r", 0.1f);
            set(p, "seq.stages", 1.0f); // 5 stages
            set(p, "seq.step1", 0.2f);
            set(p, "seq.step2", 0.45f);
            set(p, "seq.step3", 0.3f);
            set(p, "seq.step4", 0.6f);
            set(p, "seq.step5", 0.38f);
            p.patchBay().connect(s42::Inlet::VcoAVoctIn, s42::Outlet::SeqCvOut);
            p.patchBay().connect(s42::Inlet::EnvAGateIn, s42::Outlet::SeqGateOut);
        },
        [](Solar42NProcessor& p, double t)
        { set(p, "seq.pulser", (float) (t / 16.0)); },
        { "0-16s  pulser knob 0 -> 1: steps from kPulserMin to kPulserMax",
          "        (listen: is the slow end slow enough, the fast end fast enough?)" },
    });

    // ---- 5: VOLT transpose + dirty zone (kVoltTransposeOct/kVoltDirtyStart/kVoltCrossFmMax)
    scenes.push_back({
        "volt-dirty",
        "kVoltTransposeOct + kVoltDirtyStart + kVoltCrossFmMax — VOLT knob 0 -> 1 crawl",
        30.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p);
            set(p, "d1.gen5.mute", 0.0f); // all 5 gens for maximum cross-FM food
        },
        [](Solar42NProcessor& p, double t)
        { set(p, "d1.volt", (float) juce::jmin(1.0, t / 25.0)); },
        { "0-12.5s  clean transpose down (full span = kVoltTransposeOct)",
          "~12.5s   knob 0.5: cross-FM should just begin (kVoltDirtyStart)",
          "25-30s   knob 1.0 held: full dirty-zone growl (kVoltCrossFmMax)" },
    });

    // ---- 6: filter range, self-osc onset, scream (kFilterFreq*/kFilterSelfOscRes/kFilterNegDamp)
    scenes.push_back({
        "filter",
        "kFilterFreqMin/Max + kFilterSelfOscRes + kFilterNegDamp — sweep, res ramp, scream",
        36.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p);
            set(p, "filt.resL", 0.25f);
            set(p, "filt.resR", 0.25f);
        },
        [](Solar42NProcessor& p, double t)
        {
            if (t < 12) // full-range triangle sweep at moderate res
            {
                const auto ph = (float) (t / 6.0);
                const float knob = ph < 1.0f ? 1.0f - ph : ph - 1.0f;
                set(p, "filt.freqL", knob);
                set(p, "filt.freqR", knob);
            }
            else if (t < 24) // res ramp at fixed cutoff: where does it sing?
            {
                set(p, "filt.freqL", 0.45f);
                set(p, "filt.freqR", 0.45f);
                const auto res = (float) ((t - 12.0) / 12.0);
                set(p, "filt.resL", res);
                set(p, "filt.resR", res);
            }
            else // self-osc scream tracking the cutoff knob
            {
                const auto knob = (float) (0.15 + 0.7 * (t - 24.0) / 12.0);
                set(p, "filt.freqL", knob);
                set(p, "filt.freqR", knob);
            }
        },
        { "0-12s  cutoff knob 1 -> 0 -> 1 at res 0.25 (are both ends useful?)",
          "12-24s res 0 -> 1 at ~400 Hz: singing should start near knob 0.85",
          "        and turn into the bounded growl at 1.0 (no runaway, keeps bass)",
          "24-36s res 1.0, cutoff swept: the Polivoks scream" },
    });

    // ---- 7: double distortion (kDistDriveMax/kDist*V/kDistMakeup)
    scenes.push_back({
        "dist",
        "kDistDriveMax + stage knees + kDistMakeup — GAIN ramps at DIST 0 / 0.5 / 1",
        24.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p);
            set(p, "filt.freqL", 0.7f);
            set(p, "filt.freqR", 0.7f);
        },
        [](Solar42NProcessor& p, double t)
        {
            const int seg = juce::jmin(2, (int) (t / 8.0));
            set(p, "filt.dist", (float) seg * 0.5f);
            set(p, "filt.gain", (float) std::fmod(t, 8.0) / 8.0f);
        },
        { "0-8s   DIST 0 (clean path): GAIN 0 -> 1",
          "8-16s  DIST 0.5: GAIN 0 -> 1",
          "16-24s DIST 1 (full dirty): GAIN 0 -> 1",
          "        (listen: equal-loudness across the DIST sweep = kDistMakeup)" },
    });

    // ---- 8: photo-sensor lag + flicker (kSensorAttack/kSensorDecay/kSensorFlicker*)
    scenes.push_back({
        "sensor",
        "kSensorAttack/Decay + kSensorFlickerHz/Depth — square LED steps, then room light + flicker",
        24.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p);
            set(p, "d1.gen1.mod", 1.0f); // gens 1-2 follow the sensor
            set(p, "d1.gen2.mod", 1.0f);
            set(p, "lfoA.wave", 0.0f);   // square
            set(p, "lfoA.rate", 0.58f);  // ~0.5 Hz
            p.patchBay().connect(s42::Inlet::D1CvIn, s42::Outlet::LfoAOut);
        },
        [](Solar42NProcessor& p, double t)
        {
            if (t < 12) {}
            else if (t < 18)
                set(p, "room.light", (float) ((t - 12.0) / 6.0) * 0.8f);
            else
                set(p, "room.flicker", 1.0f);
        },
        { "0-12s  square LED steps: pitch jumps up fast (5 ms) and falls",
          "        with LDR memory (~120 ms) — the vactrol asymmetry",
          "12-18s room light 0 -> 0.8: ambient offset pushes the pitch floor",
          "18-24s mains flicker on: 100 Hz ripple depth on the sensor" },
    });

    // ---- 9: Papa Srapa ranges (kSrapaRate*/kSrapaPitchSpanOct/kSrapaHiShiftOct/kSrapaFmDepthOct)
    scenes.push_back({
        "srapa",
        "kSrapaRateMin/Max/x10 + kSrapaPitchSpanOct/HiShift + kSrapaFmDepthOct",
        32.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            set(p, "mix.d3.vol", 0.7f);
            set(p, "d3.hold", 1.0f);
            set(p, "d3.pitch", 0.5f);
            set(p, "d3.am", 1.0f);
        },
        [](Solar42NProcessor& p, double t)
        {
            if (t < 8) // AM chop rate: x1 range swept
                set(p, "d3.rate", (float) (t / 8.0));
            else if (t < 12) // x10 jump at the same knob
                set(p, "d3.x10", 1.0f);
            else if (t < 20) // audio-osc span, low range
            {
                set(p, "d3.x10", 0.0f);
                set(p, "d3.am", 0.0f);
                set(p, "d3.rate", 0.4f);
                set(p, "d3.pitch", (float) ((t - 12.0) / 8.0));
            }
            else if (t < 24) // hi range shift, knob parked mid so the jump stays audible
            {
                set(p, "d3.pitch", 0.3f);
                set(p, "d3.hi", 1.0f);
            }
            else // FM siren depth ramp
            {
                set(p, "d3.hi", 0.0f);
                set(p, "d3.pitch", 0.55f);
                set(p, "d3.fm", 1.0f);
                set(p, "d3.fmamt", (float) ((t - 24.0) / 8.0));
            }
        },
        { "0-8s   AM chop, RATE knob swept across x1 (kSrapaRateMin..Max)",
          "8-12s  x10 switch: same knob, ten times the chop",
          "12-20s PITCH knob swept, low range (span = kSrapaPitchSpanOct)",
          "20-24s hi switch: +kSrapaHiShiftOct",
          "24-32s FM on, AMOUNT 0 -> 1: siren interval to kSrapaFmDepthOct" },
    });

    // ---- 10: pan law + L/R tolerance skew (kFilterLrSkew, constant-power pan)
    scenes.push_back({
        "panlaw",
        "constant-power pan + kFilterLrSkew — one cluster panned across both filters",
        16.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p);
            set(p, "filt.freqL", 0.6f);
            set(p, "filt.freqR", 0.6f);
            set(p, "filt.resL", 0.55f);
            set(p, "filt.resR", 0.55f);
        },
        [](Solar42NProcessor& p, double t)
        {
            const float pan = t < 12.0 ? (float) (t / 12.0) : 0.5f;
            set(p, "mix.d1.pan", pan);
        },
        { "0-12s  PAN hard-L -> hard-R: loudness should stay flat",
          "        (constant power) while the timbre shifts slightly —",
          "        that shift IS the L/R filter tolerance (kFilterLrSkew)",
          "12-16s parked centre: both filters, the 'live stereo' width" },
    });

    // ---- 11: FX voicing, full-wet (program levels vs kFv1FullScaleVolts)
    scenes.push_back({
        "fx-wet",
        "FX voicing at BLEND=1 — gated cluster through one program per cartridge",
        40.0,
        [](Solar42NProcessor& p)
        {
            muteAll(p);
            droneCluster(p, false);
            set(p, "d1.att", 0.35f);
            set(p, "d1.rls", 0.5f);
            set(p, "fx.blend", 1.0f);
            set(p, "fx.x", 0.55f);
            set(p, "fx.y", 0.35f);
            set(p, "fx.z", 0.65f);
            set(p, "fx.cart", 0.0f); // CATHEDRAL
            set(p, "fx.progL", 0.0f);
            set(p, "fx.progR", 0.0f);
        },
        [](Solar42NProcessor& p, double t)
        {
            // 2.5 s swell every 5 s (two per cartridge); swap + 1-2-3 flip
            // land in the silent gaps.
            set(p, "d1.gate", std::fmod(t, 5.0) < 2.5 ? 1.0f : 0.0f);
            auto at = [&](double edge) { return t >= edge && t < edge + 0.05; };
            if (at(9.0))  { set(p, "fx.cart", 1.0f / 3.0f); set(p, "fx.progL", 0.5f); set(p, "fx.progR", 0.5f); }
            if (at(19.0)) { set(p, "fx.cart", 2.0f / 3.0f); set(p, "fx.progL", 1.0f); set(p, "fx.progR", 1.0f); }
            if (at(29.0)) // OCHRE one-shot: Z is its trigger threshold — drop
            {             // it so the swells fire the capture (M4 test: 0.3)
                set(p, "fx.cart", 1.0f);
                set(p, "fx.progL", 0.0f);
                set(p, "fx.progR", 0.0f);
                set(p, "fx.z", 0.12f);
            }
        },
        { "0-9s   CATHEDRAL 1 (shimmer), full wet, two swells",
          "9-19s  TIME 2 (delay-chorus)",
          "19-29s VIBROTREM 3 (chorus)",
          "29-40s OCHRE 1 (reverse one-shot) — each swell re-triggers it",
          "        (listen: wet level vs dry scenes ~ -6 dBFS = 2 V max)" },
    });

    return scenes;
}

void renderScene(const Scene& s, const juce::File& outDir)
{
    Solar42NProcessor p;
    p.prepareToPlay(kSr, kBlock);
    s.setup(p);
    // settle: latch params into the rack, then re-prime (statecheck idiom)
    {
        juce::AudioBuffer<float> buf(2, kBlock);
        juce::MidiBuffer midi;
        buf.clear();
        p.processBlock(buf, midi);
        p.prepareToPlay(kSr, kBlock);
    }

    const auto totalBlocks = (int) std::ceil(s.seconds * kSr / kBlock);
    std::vector<float> out;
    out.reserve((size_t) totalBlocks * kBlock * 2);
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    for (int b = 0; b < totalBlocks; ++b)
    {
        if (s.automate)
            s.automate(p, b * kBlock / kSr);
        buf.clear();
        p.processBlock(buf, midi);
        for (int i = 0; i < kBlock; ++i)
        {
            out.push_back(buf.getSample(0, i));
            out.push_back(buf.getSample(1, i));
        }
    }

    const auto file = outDir.getChildFile("calib-" + juce::String(s.name) + ".wav");
    writeWav16(file, out);
    std::printf("\n%s (%.0f s) — %s\n", file.getFileName().toRawUTF8(), s.seconds, s.doc);
    for (const char* line : s.timeline)
        std::printf("    %s\n", line);
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::printf("usage: solar42n_calib <outDir> [sceneName]\n\nscenes:\n");
        for (const auto& s : buildScenes())
            std::printf("  %-12s %s\n", s.name, s.doc);
        return 1;
    }

    const juce::File outDir =
        juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    outDir.createDirectory();
    const juce::String only = argc > 2 ? argv[2] : "";

    int rendered = 0;
    for (const auto& s : buildScenes())
        if (only.isEmpty() || only == s.name)
        {
            renderScene(s, outDir);
            ++rendered;
        }

    if (rendered == 0)
    {
        std::printf("no scene named '%s'\n", only.toRawUTF8());
        return 1;
    }
    std::printf("\nsolar42n_calib: %d scene(s) -> %s\n", rendered,
                outDir.getFullPathName().toRawUTF8());
    return 0;
}
