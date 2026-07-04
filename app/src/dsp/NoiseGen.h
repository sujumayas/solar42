#pragma once

#include <cstdint>

namespace s42 {

// White noise (Papa Srapa NOISE source, S&H fodder). xorshift64* — cheap,
// audio-quality flat, and deterministic: seeded from the unit-serial
// Tolerances so renders are reproducible per unit.
class NoiseGen
{
public:
    void seed(uint64_t s) noexcept { state_ = s | 1ull; }

    // Uniform white noise in [-1, 1).
    float process() noexcept
    {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        const uint64_t bits = (state_ * 0x2545F4914F6CDD1Dull) >> 11; // 53 bits
        return (float) ((double) bits * (2.0 / 9007199254740992.0) - 1.0);
    }

private:
    uint64_t state_ = 0x9E3779B97F4A7C15ull;
};

} // namespace s42
