#pragma once

#include <cmath>

#include "dsp/Smoother.h"

namespace s42 {

// The 10-channel voice mixer (manual pp5, 21). Each channel has VOL and PAN —
// and pan IS the filter routing: the knob constant-power crossfades the
// channel between the Filter L and Filter R buses. Channel order matches the
// panel silk left->right (07 §1).
struct MixerModule
{
    static constexpr int kChannels = 10;
    enum Channel { ChD1, ChD2, ChD3, ChExt, ChVcoA, ChVcoB, ChPre, ChD4, ChD5, ChD6 };

    struct Params
    {
        // Linear gains 0..1 (the plugin applies the audio taper upstream);
        // mid-travel default so a bare engine makes sound, like the M1/M2 rig.
        float vol[kChannels] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        float pan[kChannels] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                 0.5f, 0.5f, 0.5f, 0.5f, 0.5f }; // 0 = L .. 1 = R
    };

    void prepare(double sr) noexcept
    {
        for (int c = 0; c < kChannels; ++c)
        {
            gainL_[c].prepare(sr, 0.01f, 0.0f);
            gainR_[c].prepare(sr, 0.01f, 0.0f);
        }
    }

    void setParams(const Params& p) noexcept
    {
        for (int c = 0; c < kChannels; ++c)
        {
            // Constant-power pan law; targets smoothed so grabbing PAN while
            // a drone sustains stays clickless.
            const float a = p.pan[c] * 1.5707963f;
            gainL_[c].setTarget(p.vol[c] * std::cos(a));
            gainR_[c].setTarget(p.vol[c] * std::sin(a));
        }
    }

    // One sample: mix the channel inputs onto the two filter buses.
    void process(const float in[kChannels], float& busL, float& busR) noexcept
    {
        float l = 0.0f, r = 0.0f;
        for (int c = 0; c < kChannels; ++c)
        {
            l += in[c] * gainL_[c].next();
            r += in[c] * gainR_[c].next();
        }
        busL = l;
        busR = r;
    }

    Smoother gainL_[kChannels], gainR_[kChannels];
};

} // namespace s42
