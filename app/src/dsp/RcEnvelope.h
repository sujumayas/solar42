#pragma once

#include <cmath>

#include "dsp/TuningConstants.h"

namespace s42 {

// Analog-style RC envelope. The attack stage charges toward an overshoot
// target and is clipped at 1.0 — the analog "snap"; decay/release fall
// exponentially. One class serves both the drone voices' AR (sustain = 1,
// decay unused) and the VCO ADSR with LOOP (self-retrigger, env-as-LFO).
// Knob times mean: attack = time to hit full level; decay/release = time to
// come within 1 % of the target.
class RcEnvelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr) noexcept { sr_ = sr; }

    void setAr(float attackSec, float releaseSec) noexcept
    {
        set(attackSec, 0.001f, 1.0f, releaseSec);
    }

    void set(float attackSec, float decaySec, float sustain01, float releaseSec) noexcept
    {
        aCoef_ = coef(attackSec / kAttackLambda);
        dCoef_ = coef(decaySec / kFallLambda);
        rCoef_ = coef(releaseSec / kFallLambda);
        sustain_ = sustain01;
    }

    void setLoop(bool l) noexcept { loop_ = l; }

    void gate(bool on) noexcept
    {
        if (on && !gate_)
            stage_ = Stage::Attack;
        else if (!on && gate_ && stage_ != Stage::Idle)
            stage_ = Stage::Release;
        gate_ = on;
    }

    Stage stage() const noexcept { return stage_; }
    float value() const noexcept { return y_; }
    bool active() const noexcept { return stage_ != Stage::Idle || y_ > 1.0e-5f; }

    float process() noexcept
    {
        switch (stage_)
        {
            case Stage::Attack:
                y_ += (tuning::kEnvAttackOvershoot - y_) * aCoef_;
                if (y_ >= 1.0f)
                {
                    y_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;

            case Stage::Decay:
            {
                const float target = loop_ ? 0.0f : sustain_;
                y_ += (target - y_) * dCoef_;
                if (std::abs(y_ - target) < 0.001f)
                {
                    if (loop_)
                        stage_ = Stage::Attack; // self-generation: retrigger
                    else
                        stage_ = Stage::Sustain;
                }
                break;
            }

            case Stage::Sustain:
                y_ = sustain_;
                break;

            case Stage::Release:
                y_ += (0.0f - y_) * rCoef_;
                if (y_ < 0.001f)
                {
                    y_ = 0.0f;
                    stage_ = loop_ && gate_ ? Stage::Attack : Stage::Idle;
                }
                break;

            case Stage::Idle:
                break;
        }
        return y_;
    }

private:
    // Time constants per knob-second: attack traverses to clip in `time`
    // (ln(ov/(ov-1)) taus for overshoot 1.3); falls reach 1 % in `time`.
    static constexpr float kAttackLambda = 1.4663f;
    static constexpr float kFallLambda = 4.6052f;

    float coef(float tauSec) const noexcept
    {
        if (tauSec <= 0.0f)
            return 1.0f;
        return 1.0f - std::exp(-1.0f / (tauSec * (float) sr_));
    }

    double sr_ = 48000.0;
    float y_ = 0.0f, sustain_ = 1.0f;
    float aCoef_ = 1.0f, dCoef_ = 1.0f, rCoef_ = 1.0f;
    bool gate_ = false, loop_ = false;
    Stage stage_ = Stage::Idle;
};

} // namespace s42
