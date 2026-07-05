#pragma once

#include <cstdint>

#include "engine/keyboard/KbConfig.h"

// Keyboard clocking (manual p19): internal 10-300 BPM generator that
// auto-switches to the CLOCK-in jack on the first external pulse; editing
// the BPM value is the documented way back to the internal clock. The arp
// and sequencer each scale the base clock by their mult/div ratio.
namespace s42 {

struct KbClock
{
    double sr = 48000.0;
    double phase = 0.0;
    bool external = false;
    bool lastExtHigh = false;
    float extPeriod = 24000.0f; // samples between the last two external edges
    int sinceEdge = 1 << 30;
    uint32_t lastBpmEdit = 0;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        phase = 0.0;
        external = false;
        lastExtHigh = false;
        sinceEdge = 1 << 30;
    }

    // One sample. Returns true on a base clock tick.
    bool tick(float clockJackV, bool jackPatched, float bpm, uint32_t bpmEdit) noexcept
    {
        if (bpmEdit != lastBpmEdit) // manual: adjusting the tempo reverts to internal
        {
            lastBpmEdit = bpmEdit;
            external = false;
        }

        bool extEdge = false;
        if (jackPatched)
        {
            const bool hi = clockJackV > 2.5f;
            if (hi && !lastExtHigh)
            {
                extEdge = true;
                external = true;
                if (sinceEdge > 16 && sinceEdge < (int) (sr * 10.0))
                    extPeriod = (float) sinceEdge;
                sinceEdge = 0;
            }
            lastExtHigh = hi;
        }
        if (sinceEdge < (1 << 30))
            ++sinceEdge;

        if (external)
            return extEdge; // no pulses = no ticks, exactly like the hardware

        phase += (double) bpm / 60.0 / sr;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            return true;
        }
        return false;
    }

    float periodSamples(float bpm) const noexcept
    {
        return external ? extPeriod : (float) (sr * 60.0 / bpm);
    }
};

// Per-consumer clock scaler: division counts base ticks, multiplication
// schedules evenly-spaced sub-ticks from the measured base period.
struct KbClockScaler
{
    int divCount = 0;
    int multRemaining = 0;
    float multTimer = 0.0f;

    void reset() noexcept
    {
        divCount = 0;
        multRemaining = 0;
        multTimer = 0.0f;
    }

    bool tick(bool baseTick, float basePeriod, int ratioIdx) noexcept
    {
        const float ratio = kKbClockRatios[ratioIdx < 0 ? 0 :
            (ratioIdx >= kKbNumClockRatios ? kKbNumClockRatios - 1 : ratioIdx)];
        if (ratio <= 1.0f)
        {
            const int div = (int) (1.0f / ratio + 0.5f);
            if (baseTick && ++divCount >= div)
            {
                divCount = 0;
                return true;
            }
            return false;
        }

        const int mult = (int) (ratio + 0.5f);
        if (baseTick)
        {
            multRemaining = mult - 1;
            multTimer = basePeriod / ratio;
            return true;
        }
        if (multRemaining > 0 && --multTimer <= 0.0f)
        {
            --multRemaining;
            multTimer += basePeriod / ratio;
            return true;
        }
        return false;
    }
};

} // namespace s42
