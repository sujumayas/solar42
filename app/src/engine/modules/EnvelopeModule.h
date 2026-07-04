#pragma once

#include "dsp/RcEnvelope.h"
#include "dsp/Volts.h"

namespace s42 {

// Envelope A / B (manual p9): full ADSR driving the VCA that carries its VCO
// into the mixer. HOLD forces the VCA open; LOOP self-retriggers (attack from
// the decay floor / release tail — env-as-LFO). The vca cv jack sums into the
// VCA control the analog way; env out is the EG's 0..8 V (spec p4).
struct EnvelopeModule
{
    struct Params
    {
        float attackSec = 0.01f;
        float decaySec = 0.3f;
        float sustain = 0.7f;
        float releaseSec = 0.4f;
        bool hold = false;
        bool loop = false;
    };

    void prepare(double sr) noexcept { env.setSampleRate(sr); }

    void setParams(const Params& p) noexcept
    {
        params = p;
        env.set(p.attackSec, p.decaySec, p.sustain, p.releaseSec);
        env.setLoop(p.loop);
    }

    // One sample: advance the EG, return the VCA gain (0..1).
    float process(bool gate, float vcaCvVolts) noexcept
    {
        env.gate(gate || params.loop); // LOOP free-runs like the hardware's self-generation
        const float e = env.process();
        float vca = (params.hold ? 1.0f : e) + vcaCvVolts * (1.0f / 8.0f);
        return clamp01(vca);
    }

    float envOutVolts() const noexcept { return env.value() * 8.0f; }

    Params params {};
    RcEnvelope env;
};

} // namespace s42
