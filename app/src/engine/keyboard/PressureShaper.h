#pragma once

#include <cstdint>

#include "engine/keyboard/KbConfig.h"

// PRESSURE output conditioning (manual p18): raw pressure follower / ASR /
// AD / looped AD / random-per-touch, with RISE and FALL (0-255) acting as
// slew rates in the follower modes and attack/decay times in the envelope
// modes. Output is the PRESSURE jack's 0-8 V.
namespace s42 {

struct PressureShaper
{
    float out = 0.0f;    // volts, 0..8
    int envPhase = 0;    // 0 idle, 1 attack, 2 decay/release (envelope modes)
    float randV = 0.0f;  // held random target
    uint32_t rng = 0xB5297A4Du;

    // riseSec/fallSec are the mapped times for a full 8 V traverse.
    float process(float p01, bool touched, bool touchRose, KbPressureMode mode,
                  float riseSec, float fallSec, float dt) noexcept
    {
        const float up = rate(riseSec, dt);
        const float down = rate(fallSec, dt);

        switch (mode)
        {
            case KbPressureMode::Pressure:
                slewTo(p01 * 8.0f, up, down);
                break;

            case KbPressureMode::Asr:
                slewTo(touched ? 8.0f : 0.0f, up, down);
                break;

            case KbPressureMode::Ad:
            case KbPressureMode::LoopAd:
                if (touchRose)
                    envPhase = 1;
                if (envPhase == 1)
                {
                    out += up;
                    if (out >= 8.0f)
                    {
                        out = 8.0f;
                        envPhase = 2;
                    }
                }
                else if (envPhase == 2)
                {
                    out -= down;
                    if (out <= 0.0f)
                    {
                        out = 0.0f;
                        envPhase = (mode == KbPressureMode::LoopAd && touched) ? 1 : 0;
                    }
                }
                break;

            case KbPressureMode::Random:
                if (touchRose)
                {
                    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                    randV = (float) (rng & 0xFFFFu) * (8.0f / 65535.0f);
                }
                slewTo(randV, up, down); // holds between touches, like a S&H
                break;
        }
        return out;
    }

private:
    static float rate(float sec, float dt) noexcept
    {
        return sec < 1.0e-4f ? 8.0f : 8.0f * dt / sec; // volts per sample
    }

    void slewTo(float target, float up, float down) noexcept
    {
        if (out < target)
        {
            out += up;
            if (out > target)
                out = target;
        }
        else if (out > target)
        {
            out -= down;
            if (out < target)
                out = target;
        }
    }
};

} // namespace s42
