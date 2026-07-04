#pragma once

#include "dsp/NoiseGen.h"
#include "dsp/RcEnvelope.h"
#include "dsp/SampleHold.h"
#include "dsp/SchmittOsc.h"
#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"

namespace s42 {

// Papa Srapa noise voice (42N DRONE 3/6; manual p8). Two Schmitt-trigger
// relaxation oscillators: a sub-audio square modulator (RATE, x1/x10; its
// square is the voice's cv out, 0..+12 V) and an audio oscillator
// (PITCH, hi/low, ~C0..E7). The five documented modes fall out of two
// switches and a knob:
//   1. FM+AM off               -> plain drone
//   2. FM on                   -> square pitch wobble (depth = FM AMOUNT,
//                                 rate = RATE divided by the flip-flop DIVIDER)
//   3. AM on                   -> tone chopped at RATE
//   4. both                    -> chopped + wobble (sirens, birds, bursts)
//   5. NOISE knob up           -> white noise replaces the oscillator
// Plus S&H: input normalled to the voice's own white noise, sampled on rising
// edges at its clock jack (idle until something is patched there), out +-5 V.
// GATE/HOLD/ATT/RLS envelope + VCA exactly like the classic voices.
class PapaSrapaVoice
{
public:
    struct Params
    {
        float rate01 = 0.3f;   // RATE knob (sub-audio modulator)
        float fmAmt = 0.5f;    // FM AMOUNT / MOD knob
        int dividerIdx = 0;    // DIVIDER position -> tuning::kSrapaDividers
        float pitch01 = 0.5f;  // PITCH knob (audio osc)
        float noise01 = 0.0f;  // NOISE knob: osc -> white noise blend
        bool fmOn = false;
        bool amOn = false;
        bool x10 = false;      // rate range switch
        bool hi = false;       // pitch range switch
        float attackSec = 0.05f;
        float releaseSec = 0.5f;
        bool hold = false;
    };

    void prepare(double sampleRate, const Tolerances& tol, int voiceNumber);
    void setParams(const Params& p) noexcept;

    // gateIn: keypad button or GATE jack (already thresholded).
    // shPatched/shInVolts: the S&H input jack (Internal normal = own noise).
    // shClockVolts: the S&H clock jack.
    float process(bool gateIn, bool shPatched, float shInVolts,
                  float shClockVolts) noexcept; // voice out, volts

    float cvOutVolts() const noexcept { return cvOut_; }
    float envOutVolts() const noexcept { return envValue_ * 10.0f; }
    float shOutVolts() const noexcept { return shOut_; }

private:
    Params params_ {};
    SchmittOsc rateOsc_, audioOsc_;
    NoiseGen noise_;
    SampleHold sh_;
    RcEnvelope envelope_;
    Smoother rateHzSm_, pitchVSm_, fmAmtSm_, noiseSm_;
    float amCoef_ = 1.0f, chop_ = 1.0f;
    float cvOut_ = 0.0f, shOut_ = 0.0f, envValue_ = 0.0f;
    unsigned divCount_ = 0;
    bool lastRateHigh_ = false;
};

} // namespace s42
