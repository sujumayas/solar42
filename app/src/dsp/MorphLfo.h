#pragma once

namespace s42 {

// LFO A/B: a square core through a variable slew limiter — the WAVE knob
// sweeps slew time from ~0 (square) through trapezoid to half-period
// (triangle), which is the true analog morph (not a crossfade). Output is
// UNIPOLAR 0..+10 V by design: the hardware drives the drone voices'
// photo-sensor LEDs with it (manual p10).
class MorphLfo
{
public:
    void prepare(double sr) noexcept { sr_ = sr; }

    void set(float rateHz, float morph01) noexcept
    {
        rate_ = rateHz < 0.001f ? 0.001f : rateHz;
        morph_ = morph01 < 0.002f ? 0.002f : (morph01 > 1.0f ? 1.0f : morph01);
    }

    void resetPhase(float p01 = 0.0f) noexcept { phase_ = p01; }

    float process() noexcept
    {
        phase_ += (float) (rate_ / sr_);
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;

        const float target = phase_ < 0.5f ? 10.0f : 0.0f;
        // Max slope: traverse 10 V in (morph * half period) seconds.
        const float halfPeriodSamples = (float) (sr_ / (2.0 * rate_));
        const float maxStep = 10.0f / (morph_ * halfPeriodSamples);

        const float diff = target - y_;
        if (diff > maxStep)
            y_ += maxStep;
        else if (diff < -maxStep)
            y_ -= maxStep;
        else
            y_ = target;
        return y_;
    }

    float value() const noexcept { return y_; }

private:
    double sr_ = 48000.0;
    float rate_ = 1.0f, morph_ = 0.5f;
    float phase_ = 0.0f, y_ = 0.0f;
};

} // namespace s42
