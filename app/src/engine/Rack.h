#pragma once

#include "dsp/SimpleSvf.h"
#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"
#include "engine/modules/ClassicDroneVoice.h"

namespace s42 {

// The whole instrument: all modules processed in a fixed, hardware-mirroring
// order over 64-sample sub-blocks, every jack signal an audio-rate buffer in
// volts. M1 shape: one classic drone voice (DRONE 1) -> placeholder mixer tap
// -> placeholder SVF L/R -> master. The jack registry / routing table / full
// module set arrive in M2-M3.
class Rack
{
public:
    static constexpr int kSubBlock = 64;

    // Front-panel state relevant so far. The processor fills this from the
    // parameter store each block; values are already mapped to physical units.
    struct Controls
    {
        ClassicDroneVoice::Params drone1 {};
        bool drone1Gate = false;   // keypad button 1
        float filterFreqHz = 900.0f;
        float filterRes = 0.3f;
        float masterVol = 0.7f;
    };

    void prepare(double sampleRate, int maxBlockSize);
    void setControls(const Controls& c) noexcept;

    // Renders WET OUT L/R. extIn* may be nullptr (no external audio yet).
    void process(float* outL, float* outR, const float* extInL, const float* extInR,
                 int numSamples) noexcept;

    Tolerances& tolerances() noexcept { return tolerances_; }
    const ClassicDroneVoice& drone1() const noexcept { return drone1_; }

private:
    double sampleRate_ = 48000.0;
    Tolerances tolerances_;
    Controls controls_ {};

    ClassicDroneVoice drone1_;
    SimpleSvf filterL_, filterR_;
    Smoother masterSm_;
};

} // namespace s42
