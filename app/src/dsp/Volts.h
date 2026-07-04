#pragma once

#include <cmath>

// All engine signals are modeled in VOLTS, mirroring the hardware
// (see 07-42n-panel-inventory.md §3 for the per-jack ranges).
namespace s42 {

// Output calibration: hardware WET OUT peaks ~2 V; we map 2 V -> -6 dBFS.
inline constexpr float kVoltsToFs = 0.25f;

// Absolute op-amp rail. Every inlet read is soft-clamped here so feedback
// patches saturate musically instead of blowing up.
inline constexpr float kRailVolts = 12.0f;

// 1 V/oct pitch reference used by the V/oct inputs (keyboard 0..8 V).
inline constexpr float kC0Hz = 16.3515978313f; // C0, A440 tuning

inline float voltToRatio(float volts) noexcept { return std::exp2(volts); }

inline float voctToHz(float volts) noexcept { return kC0Hz * std::exp2(volts); }

// Fast tanh-like soft clamp onto the +-12 V rails (Pade-style rational).
inline float railClamp(float v) noexcept
{
    constexpr float inv = 1.0f / kRailVolts;
    float x = v * inv;
    if (x > 3.0f)  return kRailVolts;
    if (x < -3.0f) return -kRailVolts;
    const float x2 = x * x;
    return kRailVolts * (x * (27.0f + x2) / (27.0f + 9.0f * x2));
}

inline float clamp01(float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

} // namespace s42
