#pragma once

#include <cmath>

#include "dsp/TuningConstants.h"

namespace s42 {

// The classic drone voices' opto coupler (manual p7/p10): whatever CV reaches
// the voice's CV jack drives a red LED (negative CV = LED off — this is why
// the LFOs are unipolar 0..+10 V by design); the LDR under the white window
// also sees room light, and responds with vactrol-style asymmetric lag (fast
// brighten, slow LDR "memory" darken). Optional mains-lamp flicker keeps the
// documented 50 Hz hum audible when room light is up.
//
// Output is CV-bus volts 0..10 — full brightness pushes a MOD'd generator by
// 10 V * kDroneCvOctPerVolt.
class PhotoSensor
{
public:
    void prepare(double sr) noexcept
    {
        sr_ = sr;
        aCoef_ = onePole(tuning::kSensorAttack);
        dCoef_ = onePole(tuning::kSensorDecay);
        flickerInc_ = (float) (tuning::kSensorFlickerHz / sr);
    }

    void setRoom(float roomLight01, bool flicker) noexcept
    {
        room_ = roomLight01 < 0.0f ? 0.0f : (roomLight01 > 1.0f ? 1.0f : roomLight01);
        flicker_ = flicker;
    }

    float process(float ledCvVolts) noexcept
    {
        const float led = ledCvVolts <= 0.0f ? 0.0f
                        : (ledCvVolts >= 10.0f ? 1.0f : ledCvVolts * 0.1f);

        float room = room_;
        if (flicker_)
        {
            flickerPhase_ += flickerInc_;
            if (flickerPhase_ >= 1.0f)
                flickerPhase_ -= 1.0f;
            room *= 1.0f + tuning::kSensorFlickerDepth
                            * std::sin(6.2831853f * flickerPhase_);
        }

        float light = led + room;
        if (light > 1.0f)
            light = 1.0f;

        ldr_ += (light - ldr_) * (light > ldr_ ? aCoef_ : dCoef_);
        return ldr_ * 10.0f;
    }

    float brightness() const noexcept { return ldr_; } // UI window glow (M5 telemetry)

private:
    float onePole(float t) const noexcept
    {
        return 1.0f - std::exp(-1.0f / (t * (float) sr_));
    }

    double sr_ = 48000.0;
    float aCoef_ = 1.0f, dCoef_ = 0.1f;
    float ldr_ = 0.0f;
    float room_ = 0.0f, flickerPhase_ = 0.0f, flickerInc_ = 0.0f;
    bool flicker_ = false;
};

} // namespace s42
