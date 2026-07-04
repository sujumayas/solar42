#include "engine/Rack.h"

#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"

namespace s42 {

void Rack::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;
    drone1_.prepare(sampleRate, tolerances_, 1);
    filterL_.setSampleRate(sampleRate);
    filterR_.setSampleRate(sampleRate);
    masterSm_.prepare(sampleRate, 0.01f, controls_.masterVol);
    setControls(controls_);
}

void Rack::setControls(const Controls& c) noexcept
{
    controls_ = c;
    drone1_.setParams(c.drone1);
    masterSm_.setTarget(c.masterVol);

    // Placeholder dual filter: same settings both sides plus the unit's
    // component-tolerance skew (the "live stereo" trick, kept from day one).
    const float skewL = tolerances_.spread(Tolerances::id("filtL.freq"), tuning::kFilterLrSkew);
    const float skewR = tolerances_.spread(Tolerances::id("filtR.freq"), tuning::kFilterLrSkew);
    filterL_.set(c.filterFreqHz * skewL, c.filterRes);
    filterR_.set(c.filterFreqHz * skewR, c.filterRes);
}

void Rack::process(float* outL, float* outR, const float*, const float*,
                   int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        // M1: DRONE 1 straight into both filter channels at half mixer level
        // (pan center). The real 10-channel pan-crossfade mixer lands in M3.
        const float v = drone1_.process(controls_.drone1Gate, 0.0f) * 0.5f;

        const float l = filterL_.processLp(railClamp(v));
        const float r = filterR_.processLp(railClamp(v));

        const float master = masterSm_.next();
        outL[i] = l * kVoltsToFs * master;
        outR[i] = r * kVoltsToFs * master;
    }
}

} // namespace s42
