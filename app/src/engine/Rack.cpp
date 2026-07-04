#include "engine/Rack.h"

namespace s42 {

void Rack::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;
}

void Rack::process(float* outL, float* outR, const float*, const float*,
                   int numSamples) noexcept
{
    // M1 replaces this with the first classic drone voice.
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = 0.0f;
        outR[i] = 0.0f;
    }
}

} // namespace s42
