#pragma once

namespace s42 {

// Band-limited sawtooth with a 4-point polyBLEP (cubic-B-spline step
// residual). The 2-point version left folded images at ~-45 dBc in the
// audible band at 1 kHz fundamentals; the 4-point kernel pushes them below
// -60 dBc (hardware oscillators don't alias at all). Costs a 2-sample output
// latency — irrelevant for free-running generators.
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
        y1_ = y2_ = pending_ = 0.0f;
    }

    // Rising saw in [-1, 1], delayed by 2 samples.
    float processSaw(float hz) noexcept
    {
        const float f = hz < 0.0f ? 0.0f : (hz > maxHz_ ? maxHz_ : hz);
        const float dt = (float) (f * invSr_);

        phase_ += dt;
        const bool wrapped = phase_ >= 1.0f;
        if (wrapped)
            phase_ -= 1.0f;

        float cur = 2.0f * phase_ - 1.0f + pending_;
        pending_ = 0.0f;

        if (wrapped && dt > 0.0f)
        {
            // d = current sample's distance past the step, in samples [0, 1).
            const float d = phase_ / dt;
            constexpr float a = -2.0f; // saw reset jump height
            cur += a * residual(d);
            y1_ += a * residual(d - 1.0f);
            y2_ += a * residual(d - 2.0f);
            pending_ = a * residual(d + 1.0f);
        }

        const float out = y2_;
        y2_ = y1_;
        y1_ = cur;
        return out;
    }

    float phase() const noexcept { return phase_; }

private:
    // Residual r(t) = (cubic-B-spline step response) - (ideal step), t in
    // samples relative to the discontinuity, support [-2, 2].
    static float residual(float t) noexcept
    {
        if (t <= -2.0f || t >= 2.0f)
            return 0.0f;
        if (t < -1.0f)
        {
            const float x = t + 2.0f;
            const float x2 = x * x;
            return x2 * x2 * (1.0f / 24.0f);
        }
        if (t < 0.0f)
        {
            const float t2 = t * t;
            return 0.5f + t * (2.0f / 3.0f) - t2 * t * (1.0f / 3.0f) - t2 * t2 * 0.125f;
        }
        if (t < 1.0f)
        {
            const float t2 = t * t;
            return -0.5f + t * (2.0f / 3.0f) - t2 * t * (1.0f / 3.0f) + t2 * t2 * 0.125f;
        }
        const float x = 2.0f - t;
        const float x2 = x * x;
        return x2 * x2 * (-1.0f / 24.0f);
    }

    float phase_ = 0.0f;
    float y1_ = 0.0f, y2_ = 0.0f, pending_ = 0.0f;
    float maxHz_ = 20000.0f;
    double invSr_ = 1.0 / 48000.0;
};

} // namespace s42
