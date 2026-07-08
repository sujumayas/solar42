#include "engine/modules/PapaSrapaVoice.h"

#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"

#include <cmath>
#include <cstdio>

namespace s42 {

namespace
{
float expMap(float v01, float lo, float hi) noexcept
{
    return lo * std::pow(hi / lo, v01);
}
} // namespace

void PapaSrapaVoice::prepare(double sampleRate, const Tolerances& tol, int voiceNumber)
{
    rateOsc_.setSampleRate(sampleRate);
    audioOsc_.setSampleRate(sampleRate);
    envelope_.setSampleRate(sampleRate);

    char idName[32];
    std::snprintf(idName, sizeof(idName), "d%d.srapa", voiceNumber);
    const auto id = Tolerances::id(idName);

    // Charge/discharge asymmetry: each Schmitt osc gets its own non-50 % duty.
    rateOsc_.setDuty(0.5f + tuning::kSrapaDutyBias
                     + 0.5f * tuning::kSrapaDutySpread * tol.bipolar(id));
    audioOsc_.setDuty(0.5f + tuning::kSrapaDutyBias
                      + 0.5f * tuning::kSrapaDutySpread * tol.bipolar(id ^ 0xA0D10ull));
    audioOsc_.resetPhase(0.5f + 0.5f * tol.bipolar(id ^ 0xF00Dull));
    noise_.seed(id ^ tol.serial());

    rateHzSm_.prepare(sampleRate, 0.02f, 1.0f);
    pitchVSm_.prepare(sampleRate, 0.02f, tuning::kSrapaPitchSpanOct * 0.5f);
    fmAmtSm_.prepare(sampleRate, 0.02f, params_.fmAmt);
    noiseSm_.prepare(sampleRate, 0.02f, params_.noise01);
    amCoef_ = 1.0f - std::exp(-1.0f / (tuning::kSrapaAmSlewSec * (float) sampleRate));

    setParams(params_);
}

void PapaSrapaVoice::setParams(const Params& p) noexcept
{
    params_ = p;
    envelope_.setAr(p.attackSec, p.releaseSec);

    rateHzSm_.setTarget(expMap(p.rate01, tuning::kSrapaRateMin, tuning::kSrapaRateMax)
                        * (p.x10 ? tuning::kSrapaRateX10 : 1.0f));
    pitchVSm_.setTarget(p.pitch01 * tuning::kSrapaPitchSpanOct
                        + (p.hi ? tuning::kSrapaHiShiftOct : 0.0f));
    fmAmtSm_.setTarget(p.fmAmt);
    noiseSm_.setTarget(p.noise01);
}

float PapaSrapaVoice::process(bool gateIn, float cvInVolts, bool shPatched,
                              float shInVolts, float shClockVolts) noexcept
{
    envelope_.gate(gateIn || params_.hold);
    envValue_ = envelope_.process();

    // Sub-audio modulator + flip-flop divider chain (rising-edge counter).
    rateOsc_.process(rateHzSm_.next());
    const bool rateHigh = rateOsc_.isHigh();
    if (rateHigh && !lastRateHigh_)
        ++divCount_;
    lastRateHigh_ = rateHigh;
    cvOut_ = rateHigh ? 12.0f : 0.0f;

    const int div = tuning::kSrapaDividers[params_.dividerIdx];
    const bool fmHigh = div <= 1 ? rateHigh : (divCount_ & ((unsigned) div >> 1)) != 0;

    // Audio oscillator; FM jumps its pitch by up to kSrapaFmDepthOct octaves.
    // The panel cv jack drives the same pitch node (audio-rate capable).
    float pitchV = pitchVSm_.next() + cvInVolts * tuning::kSrapaCvOctPerVolt;
    if (params_.fmOn && fmHigh)
        pitchV += fmAmtSm_.next() * tuning::kSrapaFmDepthOct;
    else
        fmAmtSm_.next(); // keep the smoother running

    const float osc = audioOsc_.process(voctToHz(pitchV));
    const float white = noise_.process();

    const float noiseMix = noiseSm_.next();
    float sig = osc + (white - osc) * noiseMix;

    // AM: chop with a ~30 us slew so the edges click softly instead of thumping.
    const float chopTarget = (!params_.amOn || rateHigh) ? 1.0f : 0.0f;
    chop_ += (chopTarget - chop_) * amCoef_;
    sig *= chop_;

    // S&H runs on its own clock jack, independent of the voice's audio path.
    const float shIn = shPatched ? shInVolts : white * 5.0f;
    float held = sh_.process(shIn, shClockVolts > 2.5f);
    shOut_ = held > 5.0f ? 5.0f : (held < -5.0f ? -5.0f : held);

    return sig * tuning::kSrapaLevelVolts * envValue_;
}

} // namespace s42
