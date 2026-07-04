#pragma once

namespace s42 {

// Shared 4-point polyBLEP machinery (cubic-B-spline step residual), extracted
// from PolyBlepOsc so every discontinuous oscillator on the panel — saws,
// Schmitt squares, VCO pulse/sub/sync — corrects its steps with the same
// kernel and the same measured alias floor (< -60 dBc, test-enforced).
//
// Residual r(t) = (cubic-B-spline step response) - (ideal step), t in samples
// relative to the discontinuity, support [-2, 2].
inline float blep4Residual(float t) noexcept
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

// Correction delay line for the 4-point kernel: push naive samples, register
// steps as they happen, read the corrected signal 2 samples late (irrelevant
// for free-running oscillators). Multiple steps per sample accumulate.
class Blep4
{
public:
    void reset() noexcept { y1_ = y2_ = add_ = carry_ = next_ = 0.0f; }

    // A step of height `amp` occurred `d` samples before the sample about to
    // be pushed (d in [0, 1)). Call any number of times before push().
    void addStep(float d, float amp) noexcept
    {
        add_ += amp * blep4Residual(d);
        y1_ += amp * blep4Residual(d - 1.0f);
        y2_ += amp * blep4Residual(d - 2.0f);
        next_ += amp * blep4Residual(d + 1.0f);
    }

    float push(float naive) noexcept
    {
        const float cur = naive + add_ + carry_;
        add_ = 0.0f;
        carry_ = next_;
        next_ = 0.0f;

        const float out = y2_;
        y2_ = y1_;
        y1_ = cur;
        return out;
    }

private:
    float y1_ = 0.0f, y2_ = 0.0f;          // in-flight corrected samples
    float add_ = 0.0f;                     // corrections for the incoming sample
    float carry_ = 0.0f, next_ = 0.0f;     // correction that lands one sample later
};

} // namespace s42
