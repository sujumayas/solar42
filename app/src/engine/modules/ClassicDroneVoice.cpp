#include "engine/modules/ClassicDroneVoice.h"

#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"

#include <cstdio>

namespace s42 {

void ClassicDroneVoice::prepare(double sampleRate, const Tolerances& tol, int voiceNumber)
{
    envelope_.setSampleRate(sampleRate);
    voltSm_.prepare(sampleRate, 0.02f, 0.0f);

    for (int i = 0; i < kNumGens; ++i)
    {
        gen_[i].setSampleRate(sampleRate);
        // Free-running: each generator starts at an arbitrary phase.
        char idName[32];
        std::snprintf(idName, sizeof(idName), "d%d.gen%d", voiceNumber, i + 1);
        const auto id = Tolerances::id(idName);
        gen_[i].resetPhase(0.5f + 0.5f * tol.bipolar(id ^ 0xF00Dull));
        toleranceCentsV_[i] = tuning::kGenToleranceCents * tol.bipolar(id) / 1200.0f;

        tuneSm_[i].prepare(sampleRate, 0.02f, params_.tune[i]);
        muteGain_[i].prepare(sampleRate, 0.001f, params_.mute[i] ? 0.0f : 1.0f);
    }
}

void ClassicDroneVoice::setParams(const Params& p) noexcept
{
    params_ = p;
    envelope_.setAr(p.attackSec, p.releaseSec);
    voltSm_.setTarget(p.volt);
    for (int i = 0; i < kNumGens; ++i)
    {
        tuneSm_[i].setTarget(p.tune[i]);
        muteGain_[i].setTarget(p.mute[i] ? 0.0f : 1.0f);
    }
}

float ClassicDroneVoice::process(bool gateIn, float modCvVolts) noexcept
{
    envelope_.gate(gateIn || params_.hold);
    envValue_ = envelope_.process();

    const float volt = voltSm_.next();
    const float transposeV = -tuning::kVoltTransposeOct * volt;

    // Past the dirty-zone threshold the generators start leaking into each
    // other's pitch (negistor supply coupling) — modeled as mutual linear FM
    // driven by the previous sample's outputs.
    float fmIndex = 0.0f;
    if (volt > tuning::kVoltDirtyStart)
        fmIndex = tuning::kVoltCrossFmMax * (volt - tuning::kVoltDirtyStart)
                  / (1.0f - tuning::kVoltDirtyStart);

    float prevSum = 0.0f;
    for (int i = 0; i < kNumGens; ++i)
        prevSum += lastOut_[i];

    float sum = 0.0f;
    for (int i = 0; i < kNumGens; ++i)
    {
        const float tune = tuneSm_[i].next();
        float pitchV = kRangeLoV[i] + tune * (kRangeHiV[i] - kRangeLoV[i])
                       + toleranceCentsV_[i] + transposeV;
        if (params_.mod[i])
            pitchV += modCvVolts * tuning::kDroneCvOctPerVolt;

        float hz = voctToHz(pitchV);
        if (fmIndex > 0.0f)
        {
            const float others = (prevSum - lastOut_[i]) * (1.0f / (float) (kNumGens - 1));
            hz *= 1.0f + fmIndex * others;
        }

        const float out = gen_[i].processSaw(hz);
        lastOut_[i] = out;
        sum += out * muteGain_[i].next();
    }

    return sum * tuning::kGenLevelVolts * envValue_;
}

} // namespace s42
