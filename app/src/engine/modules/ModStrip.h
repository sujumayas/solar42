#pragma once

#include "dsp/EnvFollower.h"
#include "dsp/MorphLfo.h"
#include "dsp/Smoother.h"
#include "engine/VoltBus.h"

namespace s42 {

// The bottom-strip modulation blocks (manual pp10-12): LFO A/B, joystick,
// 5-step sequencer + pulser, preamp + envelope follower. Small enough to live
// together; each writes its outlets every sub-block.

struct LfoModule
{
    struct Params
    {
        float wave = 0.5f;   // 0 = square .. 1 = triangle (morph)
        float rateHz = 1.0f; // already range-mapped incl. x1/x6/x10
    };

    void prepare(double sr) { lfo.prepare(sr); }
    void setParams(const Params& p) noexcept { lfo.set(p.rateHz, p.wave); }

    void process(VoltBus& bus, Outlet out, int n) noexcept
    {
        float* o = bus.out(out);
        for (int i = 0; i < n; ++i)
            o[i] = lfo.process();
    }

    MorphLfo lfo;
};

struct JoystickModule
{
    struct Params
    {
        float x = 0.0f, y = 0.0f;       // stick position -1..1 (position-locking)
        float xOffset = 0.0f, yOffset = 0.0f; // offset knobs -1..1
    };

    void prepare(double sr)
    {
        xSm.prepare(sr, 0.01f, 0.0f);
        ySm.prepare(sr, 0.01f, 0.0f);
    }

    void setParams(const Params& p) noexcept
    {
        // Manual p10: offset shifts a +-5 V window across -10..+10 V
        // (knob left: -10..0, centre: -5..+5, right: 0..+10).
        xSm.setTarget(p.xOffset * 5.0f + p.x * 5.0f);
        ySm.setTarget(p.yOffset * 5.0f + p.y * 5.0f);
    }

    void process(VoltBus& bus, int n) noexcept
    {
        float* ox = bus.out(Outlet::JoyXOut);
        float* oy = bus.out(Outlet::JoyYOut);
        for (int i = 0; i < n; ++i)
        {
            ox[i] = xSm.next();
            oy[i] = ySm.next();
        }
    }

    Smoother xSm, ySm;
};

struct StepSequencerModule
{
    struct Params
    {
        float pulserHz = 2.0f;
        int stages = 5;            // 3/4/5
        float cv[5] = {};          // 0..5 V
        bool gateOn[5] = { true, true, true, true, true };
    };

    void prepare(double sr) { sr_ = sr; }
    void setParams(const Params& p) noexcept { params_ = p; }

    void process(VoltBus& bus, int n) noexcept
    {
        float* clockOut = bus.out(Outlet::SeqClockOut);
        float* cvOut = bus.out(Outlet::SeqCvOut);
        float* gateOut = bus.out(Outlet::SeqGateOut);
        const float* extClock = bus.in(Inlet::SeqClockIn);
        const bool useExt = bus.isPatched(Inlet::SeqClockIn); // jack presence switches

        for (int i = 0; i < n; ++i)
        {
            phase_ += (float) (params_.pulserHz / sr_);
            if (phase_ >= 1.0f)
                phase_ -= 1.0f;
            const bool pulserHigh = phase_ < 0.5f;
            clockOut[i] = pulserHigh ? 10.0f : -10.0f; // pulser swings +-10 V

            const bool clk = useExt ? extClock[i] > 2.5f : pulserHigh;
            if (clk && !lastClk_)
            {
                ++step_;
                if (step_ >= params_.stages)
                    step_ = 0;
            }
            lastClk_ = clk;

            cvOut[i] = params_.cv[step_];
            gateOut[i] = (clk && params_.gateOn[step_]) ? 10.0f : 0.0f;
        }
    }

    int currentStep() const noexcept { return step_; }

    Params params_ {};
    double sr_ = 48000.0;
    float phase_ = 0.0f;
    bool lastClk_ = false;
    int step_ = 0;
};

struct PreampFollowerModule
{
    struct Params
    {
        float gain = 1.0f;      // linear, up to x100 (+40 dB)
        float attackSec = 0.01f;
        float releaseSec = 0.2f;
    };

    void prepare(double sr)
    {
        follower.prepare(sr);
        gainSm.prepare(sr, 0.02f, 1.0f);
    }

    void setParams(const Params& p) noexcept
    {
        params_ = p;
        follower.set(p.attackSec, p.releaseSec);
        gainSm.setTarget(p.gain);
    }

    // hostIn: external source (nullptr when silent). Writes the preamp bus
    // (mixer channel, M3) and the follower's env/gate outs.
    void process(VoltBus& bus, const float* hostIn, int n) noexcept
    {
        float* env = bus.out(Outlet::EnvFEnvOut);
        float* gate = bus.out(Outlet::EnvFGateOut);
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm.next();
            // Host samples (+-1 fs) -> volts, then preamp gain, then rail clip.
            float v = (hostIn != nullptr ? hostIn[i] : 0.0f) * 5.0f * g;
            v = v > 5.0f ? 5.0f : (v < -5.0f ? -5.0f : v); // preamp rails
            preampBus[i] = v;

            const float e = follower.process(v) * 2.0f; // ~5 V peak -> 10 V env
            env[i] = e > 10.0f ? 10.0f : e;

            if (gateHigh_) { if (e < 1.0f) gateHigh_ = false; }
            else           { if (e > 2.0f) gateHigh_ = true; }
            gate[i] = gateHigh_ ? 8.0f : 0.0f;
        }
    }

    float preampBus[VoltBus::kSubBlock] = {}; // feeds the mixer PREAMP channel (M3)
    Params params_ {};
    EnvFollower follower;
    Smoother gainSm;
    bool gateHigh_ = false;
};

} // namespace s42
