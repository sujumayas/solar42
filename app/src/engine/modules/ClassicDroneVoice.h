#pragma once

#include "dsp/PolyBlepOsc.h"
#include "dsp/RcEnvelope.h"
#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"

namespace s42 {

// One "classic" 5-generator drone voice (42N DRONE 1/2/4/5; manual p7).
// Five free-running negistor saws with staggered factory ranges, per-generator
// MUTE and MOD, a VOLT knob that transposes everything down and pushes the
// generators into cross-FM past ~half travel, and an AR envelope + VCA with a
// HOLD gate latch. Stable when MOD is off — the movement is beating, not drift.
class ClassicDroneVoice
{
public:
    static constexpr int kNumGens = 5;

    struct Params
    {
        float tune[kNumGens] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f }; // knobs 0..1
        bool mute[kNumGens] = {};                                // true = muted
        bool mod[kNumGens] = {};    // MOD button: route the CV bus to this gen
        float volt = 0.0f;          // VOLT knob 0..1
        float attackSec = 0.05f;
        float releaseSec = 0.5f;
        bool hold = false;          // latches the gate -> endless drone
    };

    void prepare(double sampleRate, const Tolerances& tol, int voiceNumber);
    void setParams(const Params& p) noexcept;

    // gateIn: drone keypad button or GATE jack (already thresholded).
    // modCvVolts: whatever reaches the voice's pitch-mod bus (CV jack or
    // photo-sensor — selected upstream); applied only to gens with MOD on.
    float process(bool gateIn, float modCvVolts) noexcept; // voice out, volts

    float envOutVolts() const noexcept { return envValue_ * 10.0f; }
    float genLevel(int i) const noexcept { return muteGain_[i].value(); } // LED telemetry

private:
    // Factory generator ranges (manual p7) in V/oct above C0:
    // gen1 C0-G2, gen2 B1-G3, gen3 D3-B4, gen4 E4-C6, gen5 G5-D7.
    static constexpr float kRangeLoV[kNumGens] = { 0.0f, 23.0f / 12, 38.0f / 12, 52.0f / 12, 67.0f / 12 };
    static constexpr float kRangeHiV[kNumGens] = { 31.0f / 12, 43.0f / 12, 59.0f / 12, 72.0f / 12, 86.0f / 12 };

    Params params_ {};
    PolyBlepOsc gen_[kNumGens];
    Smoother tuneSm_[kNumGens];
    Smoother muteGain_[kNumGens];
    Smoother voltSm_;
    RcEnvelope envelope_;
    float toleranceCentsV_[kNumGens] = {}; // fixed per-unit detune, in volts
    float lastOut_[kNumGens] = {};         // previous-sample outputs for cross-FM
    float envValue_ = 0.0f;
};

} // namespace s42
