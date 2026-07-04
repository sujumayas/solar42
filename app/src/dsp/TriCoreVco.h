#pragma once

#include "dsp/BlepKernel.h"
#include "dsp/TuningConstants.h"

namespace s42 {

// AS3340-style triangle-core VCO (manual p9: "6 waveforms in total",
// "two more variable, morphing waveforms — saw to inverted saw, sine to
// triangle"). The MORPHING WAVEFORM rotary is modeled as one continuous
// wave01 control across four segments (the exact rotary law is not
// documented — by-ear territory, but the zone contents are):
//   [0.00, 0.25]  variable-symmetry ramp: SAW -> INVERTED SAW (morph zone 1;
//                 the apex sweeps across the period, passing triangle-ish)
//   (0.25, 0.50]  crossfade INVERTED SAW -> SINE
//   (0.50, 0.75]  polynomial shaper SINE -> TRIANGLE (morph zone 2)
//   (0.75, 1.00]  crossfade TRIANGLE -> SQUARE with PW/PWM
// Plus: -1 octave sub square (core flip-flop) and hard sync (phase reset with
// a BLEP step so sync sweeps stay clean). Linear-FM clamping (the 3340 is not
// through-zero) is the caller's job — frequency just arrives per sample here.
//
// Discontinuities (saw endpoints, square edges, sub, sync) go through the
// shared 4-point BLEP; morph-zone corner discontinuities are left naive
// (they alias an order of magnitude below step aliasing).
class TriCoreVco
{
public:
    void setSampleRate(double sr) noexcept
    {
        invSr_ = 1.0 / sr;
        maxHz_ = (float) (0.45 * sr);
    }

    void resetPhase(float phase01 = 0.0f) noexcept
    {
        phase_ = phase01;
        subHigh_ = false;
        blep_.reset();
    }

    // syncD: hard-sync edge position, in samples before "now" (0..1); pass a
    // negative value when there is no sync event this sample.
    float process(float hz, float wave01, float pw01, bool subOn, float syncD) noexcept
    {
        const float f = hz < 0.0f ? 0.0f : (hz > maxHz_ ? maxHz_ : hz);
        const float dt = (float) (f * invSr_);

        shape(wave01, pw01, dt);

        const float prev = phase_;
        phase_ += dt;

        if (dt > 0.0f)
        {
            // Square falling edge at the pulse-width crossing.
            if (sqWeight_ > 0.0f && prev < pw_ && phase_ >= pw_)
                blep_.addStep((phase_ - pw_) / dt, -2.0f * sqWeight_);

            if (phase_ >= 1.0f)
            {
                phase_ -= 1.0f;
                onWrap(phase_ / dt, subOn);
            }

            if (syncD >= 0.0f)
            {
                // The core reset syncD samples ago: output jumped from the
                // value at the pre-reset phase to the value at 0.
                float atEdge = phase_ - syncD * dt;
                if (atEdge < 0.0f)
                    atEdge += 1.0f;
                const float step = naive(0.0f, subOn) - naive(atEdge, subOn);
                phase_ = syncD * dt;
                if (step > 1.0e-4f || step < -1.0e-4f)
                    blep_.addStep(syncD, step);
            }
        }
        else if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
        }

        return blep_.push(naive(phase_, subOn));
    }

    float phase() const noexcept { return phase_; }

private:
    // Resolve the wave01/pw01 controls into segment weights for this sample.
    void shape(float wave01, float pw01, float dt) noexcept
    {
        const float w = wave01 < 0.0f ? 0.0f : (wave01 > 1.0f ? 1.0f : wave01);

        rampWeight_ = sineMorph_ = sqWeight_ = 0.0f;
        if (w <= 0.25f)
        {
            rampWeight_ = 1.0f;
            apex_ = 1.0f - w * 4.0f;          // saw (1) -> inverted saw (0)
        }
        else if (w <= 0.5f)
        {
            rampWeight_ = 1.0f - (w - 0.25f) * 4.0f; // inv saw -> sine
            apex_ = 0.0f;
            sineMorph_ = 0.0f;
        }
        else if (w <= 0.75f)
        {
            sineMorph_ = (w - 0.5f) * 4.0f;   // sine -> triangle
        }
        else
        {
            sineMorph_ = 1.0f;
            sqWeight_ = (w - 0.75f) * 4.0f;   // triangle -> square
        }

        // Keep ramp segments >= ~2 samples, except at the exact endpoints
        // where the wave becomes a true BLEP'd saw.
        const float pMin = 2.0f * dt;
        if (apex_ > 1.0f - pMin)
            apex_ = 1.0f;
        else if (apex_ < pMin)
            apex_ = 0.0f;

        pw_ = pw01 < tuning::kVcoPwMin ? tuning::kVcoPwMin
            : (pw01 > tuning::kVcoPwMax ? tuning::kVcoPwMax : pw01);
    }

    void onWrap(float d, bool subOn) noexcept
    {
        float step = 0.0f;
        if (rampWeight_ > 0.0f)
        {
            if (apex_ >= 1.0f)      // pure rising saw: +1 -> -1
                step -= 2.0f * rampWeight_;
            else if (apex_ <= 0.0f) // pure falling saw: -1 -> +1
                step += 2.0f * rampWeight_;
        }
        if (sqWeight_ > 0.0f)       // square rising edge at wrap
            step += 2.0f * sqWeight_;

        subHigh_ = !subHigh_;       // core flip-flop = -1 octave square
        if (subOn)
            step += tuning::kVcoSubMix * (subHigh_ ? 2.0f : -2.0f);

        if (step != 0.0f)
            blep_.addStep(d, step);
    }

    // Naive (unblepped) mixed waveform at a given phase.
    float naive(float ph, bool subOn) const noexcept
    {
        float v = 0.0f;

        if (rampWeight_ > 0.0f)
            v += rampWeight_ * ramp(ph);

        const float smooth = 1.0f - rampWeight_;
        if (smooth > 0.0f && sqWeight_ < 1.0f)
        {
            const float t = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
            const float sine = t * (2.0f - (t < 0.0f ? -t : t)); // parabolic sine
            const float shaped = sine + (t - sine) * sineMorph_;
            v += smooth * (1.0f - sqWeight_) * shaped;
        }
        if (sqWeight_ > 0.0f)
            v += sqWeight_ * (ph < pw_ ? 1.0f : -1.0f);

        if (subOn)
            v += tuning::kVcoSubMix * (subHigh_ ? 1.0f : -1.0f);
        return v;
    }

    // Variable-symmetry ramp: rises -1..+1 over [0, apex], falls over the
    // rest. apex==1 is a rising saw, apex==0 a falling one.
    float ramp(float ph) const noexcept
    {
        if (apex_ >= 1.0f)
            return 2.0f * ph - 1.0f;
        if (apex_ <= 0.0f)
            return 1.0f - 2.0f * ph;
        return ph < apex_ ? 2.0f * ph / apex_ - 1.0f
                          : 1.0f - 2.0f * (ph - apex_) / (1.0f - apex_);
    }

    float phase_ = 0.0f;
    float apex_ = 1.0f, pw_ = 0.5f;
    float rampWeight_ = 1.0f, sineMorph_ = 0.0f, sqWeight_ = 0.0f;
    bool subHigh_ = false;
    float maxHz_ = 20000.0f;
    double invSr_ = 1.0 / 48000.0;
    Blep4 blep_;
};

} // namespace s42
