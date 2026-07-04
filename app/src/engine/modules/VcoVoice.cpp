#include "engine/modules/VcoVoice.h"

#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"

namespace s42 {

void VcoVoice::prepare(double sampleRate, const Tolerances& tol, const char* name)
{
    core_.setSampleRate(sampleRate);
    const auto id = Tolerances::id(name);
    core_.resetPhase(0.5f + 0.5f * tol.bipolar(id ^ 0xF00Dull));
    toleranceV_ = tuning::kVcoToleranceCents * tol.bipolar(id) / 1200.0f;

    tuneVSm_.prepare(sampleRate, 0.02f, params_.tune01);
    waveSm_.prepare(sampleRate, 0.02f, params_.wave01);
    pwSm_.prepare(sampleRate, 0.02f, params_.pw01);
    pwmAmtSm_.prepare(sampleRate, 0.02f, params_.pwmAmt);
    cvAmtSm_.prepare(sampleRate, 0.02f, params_.cvAmt);

    setParams(params_);
}

void VcoVoice::setParams(const Params& p) noexcept
{
    params_ = p;
    tuneVSm_.setTarget(p.tune01);
    waveSm_.setTarget(p.wave01);
    pwSm_.setTarget(p.pw01);
    pwmAmtSm_.setTarget(p.pwmAmt);
    cvAmtSm_.setTarget(p.cvAmt);
}

float VcoVoice::process(float voctV, float cvV, float pwmV, float syncV) noexcept
{
    const float cvAmt = cvAmtSm_.next();
    const float cv = railLimit(cvV);

    float pitchV = tuning::kVcoLowBaseV + toleranceV_ + railLimit(voctV)
                   + tuneVSm_.next() + (params_.octUp ? tuning::kVcoOctUpV : 0.0f);
    if (params_.expCv)
        pitchV += cvAmt * cv;

    float hz = voctToHz(pitchV);
    if (!params_.expCv)
    {
        hz += cvAmt * cv * tuning::kVcoLinFmHzPerVolt;
        if (hz < 0.0f)
            hz = 0.0f; // non-through-zero, like the chip
    }

    // pwm jack: 10 V swings the width by up to +-0.45 at full attenuator.
    const float pw = pwSm_.next() + pwmAmtSm_.next() * railLimit(pwmV) * 0.045f;

    // Hard sync: rising edge through +1 V resets the core.
    float syncD = -1.0f;
    if (lastSync_ < 1.0f && syncV >= 1.0f && syncV > lastSync_)
        syncD = (syncV - 1.0f) / (syncV - lastSync_);
    lastSync_ = syncV;

    return core_.process(hz, waveSm_.next(), pw, params_.subOn, syncD)
           * tuning::kVcoLevelVolts;
}

} // namespace s42
