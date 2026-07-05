#pragma once

#include <array>
#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/TuningConstants.h"
#include "state/KeyboardState.h"

// Factory presets (plan §state: manual patch examples + demo-style drones).
// Each preset is code that mutates a copy of the processor's pristine default
// state tree — params by id, CABLES / KEYBOARD / CARTRIDGES children directly —
// so presets can never drift from the parameter layout, and the statecheck
// harness loads every one of them against the real processor.
//
// Values are ported from the audition scenes the user has already heard
// (M4/M6 renders); engine units convert back to knob positions through the
// same TuningConstants maps the processor uses.
namespace solar::factory {

namespace rig {

// Inverse of the processor's expMap: engine unit -> 0..1 knob position.
inline float inv(float x, float lo, float hi)
{
    return std::log(x / lo) / std::log(hi / lo);
}

inline float vol(float engineGain) { return std::sqrt(engineGain); } // audio taper

inline void set(juce::ValueTree& t, const juce::String& id, float value)
{
    for (int i = 0; i < t.getNumChildren(); ++i)
    {
        auto c = t.getChild(i);
        if (c.hasType("PARAM") && c["id"].toString() == id)
        {
            c.setProperty("value", value, nullptr);
            return;
        }
    }
    jassertfalse; // unknown parameter id — statecheck exercises every preset
}

inline void cable(juce::ValueTree& t, const char* in, const char* out)
{
    auto cables = t.getOrCreateChildWithName("CABLES", nullptr);
    juce::ValueTree c("CABLE");
    c.setProperty("in", in, nullptr);
    c.setProperty("out", out, nullptr);
    cables.appendChild(c, nullptr);
}

// Insert a cartridge, set both toggles, and mark the chips as holding what
// the panel shows (a preset never captures a mid-swap slot).
inline void cart(juce::ValueTree& t, int cartridge, int progL, int progR)
{
    set(t, "fx.cart", (float) cartridge);
    set(t, "fx.progL", (float) progL);
    set(t, "fx.progR", (float) progR);
    auto c = t.getOrCreateChildWithName("CARTRIDGES", nullptr);
    c.setProperty("cartL", cartridge, nullptr);
    c.setProperty("progL", progL, nullptr);
    c.setProperty("cartR", cartridge, nullptr);
    c.setProperty("progR", progR, nullptr);
}

// Presets state every channel they use; everything else is silent.
inline void muteMixer(juce::ValueTree& t)
{
    for (const char* ch : { "d1", "d2", "d3", "ext", "vcoA", "vcoB", "pre", "d4", "d5", "d6" })
        set(t, "mix." + juce::String(ch) + ".vol", 0.0f);
}

inline void mix(juce::ValueTree& t, const char* ch, float engineVol, float pan)
{
    set(t, "mix." + juce::String(ch) + ".vol", vol(engineVol));
    set(t, "mix." + juce::String(ch) + ".pan", pan);
}

} // namespace rig

struct Preset
{
    const char* name;
    const char* hint; // one line for the preset menu / tooltip
    void (*build)(juce::ValueTree&);
};

namespace built {

namespace tn = s42::tuning;
using namespace rig;

// The M4 audition bed: two beating clusters into CATHEDRAL shimmer, LFO A
// breathing on DRONE 4's photo-sensor, LFO B wobbling filter L.
inline void cathedralBloom(juce::ValueTree& t)
{
    set(t, "d1.gen1.tune", 0.55f);
    set(t, "d1.gen2.tune", 0.32f);
    set(t, "d1.gen3.tune", 0.22f);
    set(t, "d1.gen4.tune", 0.35f);
    set(t, "d1.gen4.mute", 0.0f); // 4 gens beating, gen 5 stays muted
    set(t, "d1.att", inv(2.5f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d1.rls", inv(6.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d1.hold", 1.0f);

    set(t, "d4.gen1.tune", 0.62f);
    set(t, "d4.gen2.tune", 0.60f);
    set(t, "d4.gen3.tune", 0.18f);
    set(t, "d4.gen1.mod", 1.0f); // gens 1-2 follow the photo-sensor
    set(t, "d4.gen2.mod", 1.0f);
    set(t, "d4.att", inv(4.0f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d4.rls", inv(8.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d4.hold", 1.0f);

    set(t, "lfoA.rate", inv(0.15f, tn::kLfoAMin, tn::kLfoAMax));
    set(t, "lfoA.wave", 1.0f); // slow triangle -> sensor LED
    set(t, "lfoB.rate", inv(0.28f, tn::kLfoBMin, tn::kLfoBMax));
    set(t, "lfoB.wave", 0.9f);

    muteMixer(t);
    mix(t, "d1", 0.55f, 0.25f);
    mix(t, "d4", 0.50f, 0.75f);

    set(t, "filt.freqL", inv(950.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resL", 0.55f);
    set(t, "filt.freqR", inv(1600.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resR", 0.45f);
    set(t, "filt.modL", 0.25f);
    set(t, "filt.dist", 0.35f);
    set(t, "filt.gain", 0.45f);

    cart(t, 0, 0, 0); // CATHEDRAL - SHIMMER both sides
    set(t, "fx.x", 0.6f);
    set(t, "fx.y", 0.15f);
    set(t, "fx.z", 0.7f);
    set(t, "fx.blend", 0.45f);

    set(t, "room.light", 0.25f);
    set(t, "master.vol", vol(0.6f));

    cable(t, "drone4.cv.in", "lfoA.out");
    cable(t, "filt.cvl.in", "lfoB.out");
}

// The full M6 audition scene: Cathedral Bloom plus Papa Srapa chirps and the
// touch keyboard as an arp voice — pulser-clocked through the patched CLOCK
// jack, V/OCT + GATE reaching the VCOs/envelopes through the stock normals.
// Touch a few plates: arp HOLD latches them into the pattern.
inline void pulserArp(juce::ValueTree& t)
{
    cathedralBloom(t);

    set(t, "d3.rate", 0.45f);
    set(t, "d3.fmamt", 0.6f);
    set(t, "d3.divider", 1.0f);
    set(t, "d3.pitch", 0.55f);
    set(t, "d3.fm", 1.0f);
    set(t, "d3.am", 1.0f);
    set(t, "d3.att", inv(1.5f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d3.rls", inv(2.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d3.hold", 1.0f);

    set(t, "vcoA.wave", 0.05f); // nearly pure saw
    set(t, "vcoA.tune", 0.4f);
    set(t, "envA.a", inv(0.02f, tn::kVcoAttMin, tn::kVcoAttMax));
    set(t, "envA.d", inv(0.5f, tn::kVcoDecMin, tn::kVcoDecMax));
    set(t, "envA.s", 0.25f);
    set(t, "envA.r", inv(0.35f, tn::kVcoRelMin, tn::kVcoRelMax));

    set(t, "vcoB.wave", 0.62f);  // sine/tri zone
    set(t, "vcoB.cvamt", 0.35f); // eats VCO A through the normal = 2-op FM
    set(t, "vcoB.tune", 0.15f);
    set(t, "envB.a", inv(0.6f, tn::kVcoAttMin, tn::kVcoAttMax));
    set(t, "envB.d", inv(1.0f, tn::kVcoDecMin, tn::kVcoDecMax));
    set(t, "envB.s", 0.5f);
    set(t, "envB.r", inv(1.2f, tn::kVcoRelMin, tn::kVcoRelMax));

    mix(t, "d3", 0.22f, 0.65f);
    mix(t, "vcoA", 0.55f, 0.45f);
    mix(t, "vcoB", 0.34f, 0.60f);

    set(t, "seq.pulser", inv(2.2f, tn::kPulserMin, tn::kPulserMax));
    set(t, "seq.stages", 2.0f); // 5 stages

    auto kbTree = t.getOrCreateChildWithName("KEYBOARD", nullptr);
    auto cfg = kbio::decode(kbTree);
    cfg.side[0].mode = s42::KbMode::Arp;
    cfg.side[0].arp.hold = true;
    cfg.side[0].arp.clockRatio = s42::kKbClockUnity + 1; // x2 the pulser
    cfg.side[0].arp.variation = 1;
    cfg.side[0].arp.intervalSemis = 12;
    cfg.portamento = 70;
    cfg.vibSpeed = 60;
    cfg.vibDepth = 18;
    cfg.vibDelay = 50;
    kbio::encode(kbTree, cfg);

    cable(t, "kb.clock.in", "seq.clock.out"); // the M6 verify patch
}

// Both Papa Srapas as an FM/AM bird pair; DRONE 3's S&H (fed by its own
// noise through the internal normal, clocked by the pulser) steps filter L.
// TIME delay-reverb spreads the chirps.
inline void srapaAviary(juce::ValueTree& t)
{
    set(t, "d3.rate", 0.45f);
    set(t, "d3.fmamt", 0.65f);
    set(t, "d3.divider", 1.0f);
    set(t, "d3.pitch", 0.55f);
    set(t, "d3.fm", 1.0f);
    set(t, "d3.am", 1.0f);
    set(t, "d3.att", inv(0.8f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d3.rls", inv(1.5f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d3.hold", 1.0f);

    set(t, "d6.rate", 0.30f);
    set(t, "d6.fmamt", 0.5f);
    set(t, "d6.divider", 2.0f);
    set(t, "d6.pitch", 0.70f);
    set(t, "d6.hi", 1.0f);
    set(t, "d6.fm", 1.0f);
    set(t, "d6.noise", 0.35f);
    set(t, "d6.att", inv(1.2f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d6.rls", inv(2.5f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d6.hold", 1.0f);

    muteMixer(t);
    mix(t, "d3", 0.5f, 0.30f);
    mix(t, "d6", 0.5f, 0.70f);

    set(t, "seq.pulser", inv(1.4f, tn::kPulserMin, tn::kPulserMax));

    set(t, "filt.freqL", inv(700.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resL", 0.5f);
    set(t, "filt.freqR", inv(1200.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resR", 0.55f);
    set(t, "filt.modL", 0.5f);
    set(t, "filt.dist", 0.3f);
    set(t, "filt.gain", 0.4f);

    cart(t, 1, 0, 0); // TIME - DELAY-REVERB
    set(t, "fx.x", 0.5f);
    set(t, "fx.y", 0.4f);
    set(t, "fx.z", 0.55f);
    set(t, "fx.blend", 0.4f);
    set(t, "master.vol", vol(0.6f));

    cable(t, "drone3.sh.clock.in", "seq.clock.out"); // pulser steps the S&H...
    cable(t, "filt.cvl.in", "drone3.sh.out");        // ...which steps filter L
}

// Manual patch example: LFO A drives DRONE 1's photo-sensor LED in a dark
// room, so the whole cluster swells with the lamp. VIBROTREM chorus widens.
inline void sensorSwell(juce::ValueTree& t)
{
    set(t, "d1.gen1.tune", 0.48f);
    set(t, "d1.gen2.tune", 0.51f);
    set(t, "d1.gen3.tune", 0.29f);
    set(t, "d1.gen1.mod", 1.0f); // gens 1-3 on the sensor bus
    set(t, "d1.gen2.mod", 1.0f);
    set(t, "d1.gen3.mod", 1.0f);
    set(t, "d1.att", inv(3.0f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d1.rls", inv(9.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d1.hold", 1.0f);

    set(t, "d5.gen1.tune", 0.66f);
    set(t, "d5.gen2.tune", 0.63f);
    set(t, "d5.gen3.tune", 0.40f);
    set(t, "d5.gen1.mod", 1.0f);
    set(t, "d5.gen2.mod", 1.0f);
    set(t, "d5.att", inv(5.0f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d5.rls", inv(12.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d5.hold", 1.0f);

    set(t, "lfoA.rate", inv(0.06f, tn::kLfoAMin, tn::kLfoAMax));
    set(t, "lfoA.wave", 1.0f);
    set(t, "lfoB.rate", inv(0.22f, tn::kLfoBMin, tn::kLfoBMax));
    set(t, "lfoB.wave", 1.0f);

    muteMixer(t);
    mix(t, "d1", 0.55f, 0.30f);
    mix(t, "d5", 0.48f, 0.70f);

    set(t, "filt.freqL", inv(1100.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resL", 0.35f);
    set(t, "filt.freqR", inv(1400.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resR", 0.30f);
    set(t, "filt.dist", 0.25f);
    set(t, "filt.gain", 0.35f);

    cart(t, 2, 2, 2); // VIBROTREM - CHORUS both sides
    set(t, "fx.x", 0.45f);
    set(t, "fx.y", 0.5f);
    set(t, "fx.z", 0.5f);
    set(t, "fx.blend", 0.35f);

    set(t, "room.light", 0.12f); // dark room: the patched LED owns the LDR
    set(t, "master.vol", vol(0.6f));

    cable(t, "drone1.cv.in", "lfoA.out");
    cable(t, "drone5.cv.in", "lfoB.out");
}

// OCHRE's free-run loop chewing on a sparse two-cluster pad — reversed air.
inline void reverseAir(juce::ValueTree& t)
{
    set(t, "d2.gen1.tune", 0.42f);
    set(t, "d2.gen2.tune", 0.45f);
    set(t, "d2.gen3.mute", 1.0f); // just two gens beating slowly
    set(t, "d2.att", inv(6.0f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d2.rls", inv(15.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d2.hold", 1.0f);

    set(t, "d5.gen1.tune", 0.58f);
    set(t, "d5.gen2.tune", 0.61f);
    set(t, "d5.gen3.tune", 0.33f);
    set(t, "d5.att", inv(4.0f, tn::kDroneAttMin, tn::kDroneAttMax));
    set(t, "d5.rls", inv(10.0f, tn::kDroneRlsMin, tn::kDroneRlsMax));
    set(t, "d5.hold", 1.0f);

    set(t, "lfoA.rate", inv(0.08f, tn::kLfoAMin, tn::kLfoAMax));
    set(t, "lfoA.wave", 1.0f);

    muteMixer(t);
    mix(t, "d2", 0.5f, 0.35f);
    mix(t, "d5", 0.44f, 0.65f);

    set(t, "filt.freqL", inv(600.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resL", 0.4f);
    set(t, "filt.freqR", inv(800.0f, tn::kFilterFreqMin, tn::kFilterFreqMax));
    set(t, "filt.resR", 0.4f);
    set(t, "filt.modL", 0.2f);
    set(t, "filt.gain", 0.3f);

    cart(t, 3, 2, 2); // OCHRE - free-run loop
    set(t, "fx.x", 0.5f);
    set(t, "fx.y", 0.5f);
    set(t, "fx.z", 0.6f);
    set(t, "fx.blend", 0.55f);

    set(t, "room.light", 0.3f);
    set(t, "master.vol", vol(0.6f));

    cable(t, "filt.cvl.in", "lfoA.out"); // slow drift under the loop
}

inline void init(juce::ValueTree&) {} // pristine defaults, factory normals only

} // namespace built

inline const std::array<Preset, 6>& presets()
{
    static const std::array<Preset, 6> list = { {
        { "Init", "Factory defaults - hardware normals, nothing held", &built::init },
        { "Cathedral Bloom", "Two beating clusters into CATHEDRAL shimmer (the M4 bed)",
          &built::cathedralBloom },
        { "Pulser Arp", "The M6 scene - latch plates and the pulser clocks the arp",
          &built::pulserArp },
        { "Srapa Aviary", "Papa Srapa FM/AM birds, S&H stepping filter L, TIME behind",
          &built::srapaAviary },
        { "Sensor Swell", "LFO A on DRONE 1's sensor LED in a dark room, VIBROTREM width",
          &built::sensorSwell },
        { "Reverse Air", "Sparse pads through OCHRE's free-run reverse loop",
          &built::reverseAir },
    } };
    return list;
}

} // namespace solar::factory
