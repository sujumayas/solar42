#pragma once

#include <cmath>

namespace s42 {

// Preamp envelope follower: full-wave rectify -> peak follower with separate
// attack/release one-poles. Env out 0..10 V; the gate comparator (0..8 V,
// with hysteresis) lives in the module.
class EnvFollower
{
public:
    void prepare(double sr) noexcept { sr_ = sr; }

    void set(float attackSec, float releaseSec) noexcept
    {
        aCoef_ = coef(attackSec);
        rCoef_ = coef(releaseSec);
    }

    float process(float x) noexcept
    {
        const float rect = x < 0.0f ? -x : x;
        y_ += (rect - y_) * (rect > y_ ? aCoef_ : rCoef_);
        return y_;
    }

    float value() const noexcept { return y_; }

private:
    float coef(float t) const noexcept
    {
        if (t <= 0.0001f)
            return 1.0f;
        return 1.0f - std::exp(-1.0f / (t * (float) sr_));
    }

    double sr_ = 48000.0;
    float y_ = 0.0f, aCoef_ = 1.0f, rCoef_ = 0.1f;
};

} // namespace s42
