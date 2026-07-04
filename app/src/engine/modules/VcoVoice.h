#pragma once

#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"
#include "dsp/TriCoreVco.h"

namespace s42 {

// VCO A / B (manual p9, 42N deltas p26): AS3340 triangle core with the
// MORPHING WAVEFORM rotary (see TriCoreVco), SHAPE pw + pwm-jack attenuator,
// -1 sub switch, oct+3/low octave switch, TUNE over one octave, and the
// lin/exp switch selecting the CV jack's response: exp = V/oct scaled by the
// CV AMT attenuator, lin = linear FM that clamps at 0 Hz (a 3340 is not
// through-zero). VCO A also has the hard-sync input.
class VcoVoice
{
public:
    struct Params
    {
        float cvAmt = 0.5f;   // CV attenuator (VCO B's default FM depth from VCO A)
        float tune01 = 0.5f;  // 1 octave span
        float wave01 = 0.0f;  // MORPHING WAVEFORM rotary
        float pwmAmt = 0.0f;  // pwm jack attenuator
        float pw01 = 0.5f;    // SHAPE / pulse width
        bool octUp = false;   // oct+3 / low
        bool subOn = false;   // -1 sub-oscillator
        bool expCv = false;   // lin/exp CV response
    };

    void prepare(double sampleRate, const Tolerances& tol, const char* name);
    void setParams(const Params& p) noexcept;

    // All jack inputs in volts; returns the raw oscillator output (+-5 V) —
    // the VCA into the mixer belongs to Envelope A/B, not the VCO.
    float process(float voctV, float cvV, float pwmV, float syncV) noexcept;

private:
    Params params_ {};
    TriCoreVco core_;
    Smoother tuneVSm_, waveSm_, pwSm_, pwmAmtSm_, cvAmtSm_;
    float toleranceV_ = 0.0f;
    float lastSync_ = 0.0f;
};

} // namespace s42
