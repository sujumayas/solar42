#pragma once

#include "dsp/TuningConstants.h"

namespace s42 {

// Asymmetric soft clip: rational tanh-shape with different knees for the
// positive and negative half — the asymmetry is what puts even harmonics into
// the Polivoks drive and the post-filter distortion.
inline float asymClip(float v, float posKneeV, float negKneeV) noexcept
{
    const float knee = v >= 0.0f ? posKneeV : negKneeV;
    const float x = v / knee;
    if (x > 3.0f)
        return knee;
    if (x < -3.0f)
        return -knee;
    const float x2 = x * x;
    return knee * (x * (27.0f + x2) / (27.0f + 9.0f * x2));
}

// The 42N's post-filter "double distortion" (manual p21): GAIN drives two
// cascaded asymmetric stages, DIST balances clean vs distorted signal.
// Stateless; shared L/R parameters, called once per side per sample.
inline float distGain(float xVolts, float gain01, float dist01) noexcept
{
    if (dist01 <= 0.0f)
        return xVolts;

    const float drive = 1.0f + gain01 * tuning::kDistDriveMax;
    float y = asymClip(xVolts * drive, tuning::kDistStage1PosV, tuning::kDistStage1NegV);
    y = asymClip(y, tuning::kDistStage2PosV, tuning::kDistStage2NegV);
    y *= tuning::kDistMakeup * (1.0f + gain01); // keep DIST sweeps roughly equal-loud

    return xVolts + (y - xVolts) * dist01;
}

} // namespace s42
