#include "engine/Rack.h"

#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"

#include <algorithm>

namespace s42 {

void Rack::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    lfoA_.prepare(sampleRate);
    lfoB_.prepare(sampleRate);
    joy_.prepare(sampleRate);
    seq_.prepare(sampleRate);
    preamp_.prepare(sampleRate);
    drone1_.prepare(sampleRate, tolerances_, 1);
    filterL_.setSampleRate(sampleRate);
    filterR_.setSampleRate(sampleRate);
    masterSm_.prepare(sampleRate, 0.01f, controls_.masterVol);

    bus_.clearAll();
    setControls(controls_);
}

void Rack::setControls(const Controls& c) noexcept
{
    controls_ = c;

    lfoA_.setParams(c.lfoA);
    lfoB_.setParams(c.lfoB);
    joy_.setParams(c.joy);
    seq_.setParams(c.seq);
    preamp_.setParams(c.preamp);
    drone1_.setParams(c.drone1);
    masterSm_.setTarget(c.masterVol);

    const float skewL = tolerances_.spread(Tolerances::id("filtL.freq"), tuning::kFilterLrSkew);
    const float skewR = tolerances_.spread(Tolerances::id("filtR.freq"), tuning::kFilterLrSkew);
    filterL_.set(c.filterFreqHz * skewL, c.filterRes);
    filterR_.set(c.filterFreqHz * skewR, c.filterRes);
}

void Rack::drainCommands() noexcept
{
    PatchCmd cmd;
    while (cmdQueue_.pop(cmd))
    {
        if (cmd.outlet == VoltBus::kNoSource)
            bus_.unpatch((Inlet) cmd.inlet);
        else
            bus_.patch((Inlet) cmd.inlet, (Outlet) cmd.outlet);
    }
}

void Rack::process(float* outL, float* outR, const float* extInL, const float* extInR,
                   int numSamples) noexcept
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min(kSubBlock, numSamples - done);
        processSubBlock(outL + done, outR + done,
                        extInL != nullptr ? extInL + done : nullptr,
                        extInR != nullptr ? extInR + done : nullptr, n);
        done += n;
    }
}

void Rack::processSubBlock(float* outL, float* outR, const float* extInL,
                           const float* /*extInR*/, int n) noexcept
{
    drainCommands();

    // Fixed hardware-mirroring order (backward cables read the previous
    // sub-block — that's the feedback semantics, see VoltBus).
    lfoA_.process(bus_, Outlet::LfoAOut, n);
    lfoB_.process(bus_, Outlet::LfoBOut, n);
    joy_.process(bus_, n);
    seq_.process(bus_, n);
    preamp_.process(bus_, extInL, n);

    // DRONE 1: gate = keypad button OR gate-jack voltage; pitch-mod CV bus
    // from its CV jack (rail-clamped; the photo-sensor joins in M3).
    {
        const float* gateJack = bus_.in(Inlet::D1GateIn);
        const float* cvJack = bus_.in(Inlet::D1CvIn);
        float* envOut = bus_.out(Outlet::D1EnvOut);

        for (int i = 0; i < n; ++i)
        {
            const bool gate = controls_.drone1Key || gateJack[i] > 2.5f;
            const float v = drone1_.process(gate, railClamp(cvJack[i])) * 0.5f;
            envOut[i] = drone1_.envOutVolts();

            const float l = filterL_.processLp(railClamp(v));
            const float r = filterR_.processLp(railClamp(v));

            const float master = masterSm_.next();
            outL[i] = l * kVoltsToFs * master;
            outR[i] = r * kVoltsToFs * master;
        }
    }
}

} // namespace s42
