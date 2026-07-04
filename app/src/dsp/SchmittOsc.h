#pragma once

#include "dsp/BlepKernel.h"

namespace s42 {

// Papa Srapa's building block: a Schmitt-trigger RC relaxation oscillator
// (manual p8 — "two Trigger-Schmidt oscillators"). The capacitor charges to
// the upper threshold and discharges to the lower one, so the square is never
// 50 % duty (charge and discharge resistances differ) and the rate follows
// the control voltage roughly linearly. Both edges are polyBLEP-corrected;
// output is +-1 with the same 2-sample kernel delay as PolyBlepOsc.
class SchmittOsc
{
public:
    void setSampleRate(double sr) noexcept
    {
        invSr_ = 1.0 / sr;
        maxHz_ = (float) (0.45 * sr);
    }

    // duty = fraction of the period spent high (the charge segment).
    void setDuty(float duty01) noexcept
    {
        duty_ = duty01 < 0.05f ? 0.05f : (duty01 > 0.95f ? 0.95f : duty01);
    }

    void resetPhase(float phase01 = 0.0f) noexcept
    {
        phase_ = phase01;
        blep_.reset();
    }

    // hz may change every sample (voltage-controlled rate).
    float process(float hz) noexcept
    {
        const float f = hz < 0.0f ? 0.0f : (hz > maxHz_ ? maxHz_ : hz);
        const float dt = (float) (f * invSr_);

        const float prev = phase_;
        phase_ += dt;

        if (dt > 0.0f)
        {
            if (prev < duty_ && phase_ >= duty_)                  // falling edge
                blep_.addStep((phase_ - duty_) / dt, -2.0f);
            if (phase_ >= 1.0f)
            {
                phase_ -= 1.0f;
                blep_.addStep(phase_ / dt, 2.0f);                 // rising edge
                if (phase_ >= duty_) // pathological dt > duty: catch the fall too
                    blep_.addStep((phase_ - duty_) / dt, -2.0f);
            }
        }
        else if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
        }

        high_ = phase_ < duty_;
        return blep_.push(high_ ? 1.0f : -1.0f);
    }

    // Comparator state *before* the kernel delay — use for clocking logic
    // (S&H, divider flip-flops), not for audio.
    bool isHigh() const noexcept { return high_; }

private:
    float phase_ = 0.0f, duty_ = 0.55f;
    bool high_ = true;
    float maxHz_ = 20000.0f;
    double invSr_ = 1.0 / 48000.0;
    Blep4 blep_;
};

} // namespace s42
