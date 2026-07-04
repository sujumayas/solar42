#pragma once

#include "dsp/SimpleSvf.h"
#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"
#include "engine/CommandQueue.h"
#include "engine/VoltBus.h"
#include "engine/modules/ClassicDroneVoice.h"
#include "engine/modules/ModStrip.h"

namespace s42 {

// The whole instrument. Fixed hardware-mirroring process order over 64-sample
// sub-blocks; every jack signal is an audio-rate volt buffer in the VoltBus;
// patch edits arrive through a lock-free command queue and are applied at
// sub-block boundaries.
//
// M2 module set: LFO A/B, joystick, 5-step sequencer + pulser, preamp +
// envelope follower, DRONE 1, placeholder dual SVF, master. Remaining voices,
// the real Polivoks filter, and the mixer land in M3.
class Rack
{
public:
    static constexpr int kSubBlock = VoltBus::kSubBlock;

    struct Controls
    {
        ClassicDroneVoice::Params drone1 {};
        bool drone1Key = false; // keypad button 1 (OR'd with the GATE jack)
        LfoModule::Params lfoA { 0.5f, 0.2f };
        LfoModule::Params lfoB { 0.5f, 4.0f };
        JoystickModule::Params joy {};
        StepSequencerModule::Params seq {};
        PreampFollowerModule::Params preamp {};
        float filterFreqHz = 900.0f;
        float filterRes = 0.3f;
        float masterVol = 0.7f;
    };

    void prepare(double sampleRate, int maxBlockSize);
    void setControls(const Controls& c) noexcept;

    // Message-thread side of patching. Safe to call while audio runs.
    bool requestPatch(Inlet i, Outlet o) noexcept { return cmdQueue_.push({ (int16_t) i, (int16_t) o }); }
    bool requestUnpatch(Inlet i) noexcept { return cmdQueue_.push({ (int16_t) i, VoltBus::kNoSource }); }

    void process(float* outL, float* outR, const float* extInL, const float* extInR,
                 int numSamples) noexcept;

    Tolerances& tolerances() noexcept { return tolerances_; }
    const VoltBus& bus() const noexcept { return bus_; }

private:
    void processSubBlock(float* outL, float* outR, const float* extInL,
                         const float* extInR, int n) noexcept;
    void drainCommands() noexcept;

    double sampleRate_ = 48000.0;
    Tolerances tolerances_;
    Controls controls_ {};

    VoltBus bus_;
    CommandQueue cmdQueue_;

    LfoModule lfoA_, lfoB_;
    JoystickModule joy_;
    StepSequencerModule seq_;
    PreampFollowerModule preamp_;
    ClassicDroneVoice drone1_;
    SimpleSvf filterL_, filterR_;
    Smoother masterSm_;
};

} // namespace s42
