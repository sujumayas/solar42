#pragma once

#include "dsp/BlepKernel.h"

namespace s42 {

// Band-limited sawtooth with the shared 4-point polyBLEP (see BlepKernel.h).
// The 2-point version left folded images at ~-45 dBc in the audible band at
// 1 kHz fundamentals; the 4-point kernel pushes them below -60 dBc (hardware
// oscillators don't alias at all). Costs a 2-sample output latency —
// irrelevant for free-running generators.
//
// The frequency argument may change every sample, which is what makes this
// usable for the classic drone voices' VOLT cross-FM "dirty zone".
class PolyBlepOsc
{
public:
    void setSampleRate(double sr) noexcept
    {
        invSr_ = 1.0 / sr;
        maxHz_ = (float) (0.45 * sr);
    }

    void resetPhase(float phase01 = 0.0f) noexcept
    {
        phase_ = phase01;
        blep_.reset();
    }

    // Rising saw in [-1, 1], delayed by 2 samples.
    float processSaw(float hz) noexcept
    {
        const float f = hz < 0.0f ? 0.0f : (hz > maxHz_ ? maxHz_ : hz);
        const float dt = (float) (f * invSr_);

        phase_ += dt;
        if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
            if (dt > 0.0f)
                blep_.addStep(phase_ / dt, -2.0f); // saw reset jump
        }

        return blep_.push(2.0f * phase_ - 1.0f);
    }

    float phase() const noexcept { return phase_; }

private:
    float phase_ = 0.0f;
    float maxHz_ = 20000.0f;
    double invSr_ = 1.0 / 48000.0;
    Blep4 blep_;
};

} // namespace s42
