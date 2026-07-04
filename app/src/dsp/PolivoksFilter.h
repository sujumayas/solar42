#pragma once

#include <cmath>
#include <cstdint>

#include "dsp/TuningConstants.h"
#include "dsp/Waveshaper.h"

namespace s42 {

// The 42N's Polivoks-type 12 dB filter, one instance per channel. Nonlinear
// 2-pole ZDF/TPT state-variable core with the three documented traits
// (manual p21 + Polivoks lore):
//   a) asymmetric clip on the input drive ("dirty and angry" even at low RES),
//   b) hard-ish saturator on the bandpass state — the resonance feedback path —
//      so resonance screams into *bounded* self-oscillation,
//   c) no bass loss when resonance rises ("you will not lose low frequencies
//      when increasing resonance") — an SVF keeps its passband at unity, so
//      the topology itself delivers the documented trait.
// Past RES ≈ kFilterSelfOscRes the damping goes slightly negative: the filter
// truly self-oscillates and the feedback saturator sets the amplitude. A
// -120 dB op-amp noise floor seeds the oscillation like thermal noise does on
// the hardware.
//
// Cutoff is a per-sample argument (audio-rate filter FM through the CV jacks
// is part of the instrument).
class PolivoksFilter
{
public:
    void setSampleRate(double sr) noexcept
    {
        sr_ = sr;
        maxFc_ = (float) (0.47 * sr);
    }

    void seedNoise(uint64_t s) noexcept { noiseState_ = (uint32_t) (s | 1u); }

    void setRes(float res01) noexcept
    {
        const float r = res01 < 0.0f ? 0.0f : (res01 > 1.0f ? 1.0f : res01);
        constexpr float so = tuning::kFilterSelfOscRes;
        k_ = r < so ? 2.0f * (1.0f - r / so)
                    : -tuning::kFilterNegDamp * (r - so) / (1.0f - so);
    }

    void setMode(bool bandpass) noexcept { bp_ = bandpass; }

    void reset() noexcept { ic1_ = ic2_ = 0.0f; }

    float process(float xVolts, float cutoffHz) noexcept
    {
        const float fc = cutoffHz < 8.0f ? 8.0f : (cutoffHz > maxFc_ ? maxFc_ : cutoffHz);
        const float g = std::tan(3.14159265f * fc / (float) sr_);

        // (a) asymmetric input drive.
        float x = asymClip(xVolts * tuning::kFilterDrive,
                           tuning::kFilterDrivePosV, tuning::kFilterDriveNegV);
        x += noise(); // thermal seed for self-oscillation

        // Linear ZDF prediction, then (b) saturate the resonance path.
        const float a1 = 1.0f / (1.0f + g * (g + k_));
        float v1 = a1 * (ic1_ + g * (x - ic2_));
        v1 = asymClip(v1, tuning::kFilterFbClipV, tuning::kFilterFbClipV);
        const float v2 = ic2_ + g * v1;

        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        flush(ic1_);
        flush(ic2_);

        return bp_ ? v1 : v2;
    }

private:
    static void flush(float& v) noexcept
    {
        if (v > -1.0e-18f && v < 1.0e-18f)
            v = 0.0f;
    }

    float noise() noexcept
    {
        noiseState_ = noiseState_ * 1664525u + 1013904223u;
        return ((float) (int32_t) noiseState_) * 1.0e-15f; // ~1e-6 V pk
    }

    double sr_ = 48000.0;
    float maxFc_ = 22000.0f;
    float k_ = 2.0f;
    float ic1_ = 0.0f, ic2_ = 0.0f;
    bool bp_ = false;
    uint32_t noiseState_ = 22222u;
};

} // namespace s42
