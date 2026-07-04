#pragma once

namespace s42 {

// Sample & hold (one per Papa Srapa voice). Samples the input on the clock's
// rising edge only — no internal clock; the hardware S&H sits idle until
// something is patched into its clock jack.
class SampleHold
{
public:
    float process(float in, bool clockHigh) noexcept
    {
        if (clockHigh && !lastClock_)
            held_ = in;
        lastClock_ = clockHigh;
        return held_;
    }

    float held() const noexcept { return held_; }

private:
    float held_ = 0.0f;
    bool lastClock_ = false;
};

} // namespace s42
