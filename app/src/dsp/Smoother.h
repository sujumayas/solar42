#pragma once

#include <cmath>

namespace s42 {

// One-pole parameter smoother (zipper-noise removal, mute declick ramps).
class Smoother
{
public:
    void prepare(double sampleRate, float timeSec, float initial = 0.0f) noexcept
    {
        coef_ = timeSec <= 0.0f ? 1.0f
                                : 1.0f - std::exp(-1.0f / (timeSec * (float) sampleRate));
        y_ = target_ = initial;
    }

    void setTarget(float t) noexcept { target_ = t; }
    void snap(float v) noexcept { y_ = target_ = v; }

    float next() noexcept
    {
        y_ += (target_ - y_) * coef_;
        return y_;
    }

    float value() const noexcept { return y_; }

private:
    float y_ = 0.0f, target_ = 0.0f, coef_ = 1.0f;
};

} // namespace s42
