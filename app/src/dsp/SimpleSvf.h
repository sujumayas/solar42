#pragma once

#include <cmath>

namespace s42 {

// Linear TPT/ZDF state-variable filter (Zavalishin). M1-M2 placeholder for the
// signal path; the nonlinear Polivoks model (drive + feedback saturation +
// no-bass-loss) replaces/extends this in M3.
class SimpleSvf
{
public:
    void setSampleRate(double sr) noexcept { sr_ = sr; }

    void set(float cutoffHz, float res01) noexcept
    {
        const float fc = cutoffHz < 10.0f ? 10.0f
                       : (cutoffHz > (float) sr_ * 0.49f ? (float) sr_ * 0.49f : cutoffHz);
        g_ = std::tan(3.14159265f * fc / (float) sr_);
        const float r = res01 < 0.0f ? 0.0f : (res01 > 0.98f ? 0.98f : res01);
        k_ = 2.0f - 2.0f * r;
    }

    void reset() noexcept { ic1_ = ic2_ = 0.0f; }

    float processLp(float x) noexcept
    {
        const float a1 = 1.0f / (1.0f + g_ * (g_ + k_));
        const float v1 = a1 * (ic1_ + g_ * (x - ic2_));
        const float v2 = ic2_ + g_ * v1;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        return v2;
    }

private:
    double sr_ = 48000.0;
    float g_ = 0.1f, k_ = 2.0f;
    float ic1_ = 0.0f, ic2_ = 0.0f;
};

} // namespace s42
