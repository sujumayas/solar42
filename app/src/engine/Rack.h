#pragma once

#include "dsp/Tolerances.h"

namespace s42 {

// The whole instrument: all modules processed in a fixed, hardware-mirroring
// order over 64-sample sub-blocks, every jack signal an audio-rate buffer in
// volts (see plan §Engine architecture; jack registry arrives in M2).
//
// M0: silent shell exposing the prepare/process surface the plugin binds to.
class Rack
{
public:
    static constexpr int kSubBlock = 64;

    void prepare(double sampleRate, int maxBlockSize);

    // Renders WET OUT L/R into the given buffers (already sized to numSamples).
    // extIn* may be nullptr (no external audio / preamp source connected).
    void process(float* outL, float* outR, const float* extInL, const float* extInR,
                 int numSamples) noexcept;

    Tolerances& tolerances() noexcept { return tolerances_; }

private:
    double sampleRate_ = 48000.0;
    Tolerances tolerances_;
};

} // namespace s42
