#pragma once

#include "dsp/PhotoSensor.h"
#include "dsp/PolivoksFilter.h"
#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"
#include "engine/CommandQueue.h"
#include "engine/Telemetry.h"
#include "engine/VoltBus.h"
#include "engine/modules/ClassicDroneVoice.h"
#include "engine/modules/Effector.h"
#include "engine/modules/EnvelopeModule.h"
#include "engine/modules/Mixer.h"
#include "engine/modules/ModStrip.h"
#include "engine/keyboard/TouchKeyboard.h"
#include "engine/modules/PapaSrapaVoice.h"
#include "engine/modules/VcoVoice.h"

namespace s42 {

// The whole instrument. Fixed hardware-mirroring process order over 64-sample
// sub-blocks; every jack signal is an audio-rate volt buffer in the VoltBus;
// patch edits arrive through a lock-free command queue and are applied at
// sub-block boundaries.
//
// Module set: touch keyboard (M6, the digital brain — runs first, its V/oct
// and gates are normalled into the VCOs/envelopes), LFO A/B, joystick,
// sequencer, preamp + follower, DRONE 1/2/4/5 (classic, with photo-sensors),
// DRONE 3/6 (Papa Srapa), VCO A/B + Envelope A/B, 10-channel mixer (pan =
// filter routing), dual nonlinear Polivoks filter + shared DIST/GAIN, dual
// FV-1 effector (M4), master.
class Rack
{
public:
    static constexpr int kSubBlock = VoltBus::kSubBlock;
    static constexpr int kClassicVoices = 4; // DRONE 1, 2, 4, 5
    static constexpr int kSrapaVoices = 2;   // DRONE 3, 6

    struct FilterParams
    {
        float freqHzL = 900.0f, freqHzR = 900.0f;
        float resL = 0.3f, resR = 0.3f;
        bool bpL = false, bpR = false;   // BP/LP mode switches
        float modL = 0.0f, modR = 0.0f;  // CV amount knobs
        bool link = false;               // CV L controls both channels
        float dist = 0.0f;               // clean/dirty balance (shared)
        float gain = 0.3f;               // distortion amount (shared)
    };

    struct Controls
    {
        ClassicDroneVoice::Params drone[kClassicVoices] {};
        bool droneKey[kClassicVoices] = {};   // keypad buttons 1, 2, 4, 5
        PapaSrapaVoice::Params srapa[kSrapaVoices] {};
        bool srapaKey[kSrapaVoices] = {};     // keypad buttons 3, 6
        VcoVoice::Params vcoA {}, vcoB {};
        EnvelopeModule::Params envA {}, envB {};
        LfoModule::Params lfoA { 0.5f, 0.2f };
        LfoModule::Params lfoB { 0.5f, 4.0f };
        JoystickModule::Params joy {};
        StepSequencerModule::Params seq {};
        PreampFollowerModule::Params preamp {};
        MixerModule::Params mixer {};
        FilterParams filter {};
        EffectorModule::Params fx {};
        KbConfig kb {};                       // touch keyboard firmware settings
        KbTouch kbTouch {};                   // live plate/button gestures
        float roomLight = 0.35f;              // ambient light on the photo-sensors
        bool mainsFlicker = false;
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
    const Telemetry& telemetry() const noexcept { return telemetry_; }

private:
    void processSubBlock(float* outL, float* outR, const float* extInL,
                         const float* extInR, int n) noexcept;
    void drainCommands() noexcept;
    void publishTelemetry(float peakL, float peakR, int n) noexcept;

    double sampleRate_ = 48000.0;
    Tolerances tolerances_;
    Controls controls_ {};

    VoltBus bus_;
    CommandQueue cmdQueue_;
    Telemetry telemetry_;

    TouchKeyboard keyboard_;
    LfoModule lfoA_, lfoB_;
    JoystickModule joy_;
    StepSequencerModule seq_;
    PreampFollowerModule preamp_;

    ClassicDroneVoice drones_[kClassicVoices];
    PhotoSensor sensors_[kClassicVoices];
    PapaSrapaVoice srapas_[kSrapaVoices];
    VcoVoice vcoA_, vcoB_;
    EnvelopeModule envA_, envB_;

    MixerModule mixer_;
    PolivoksFilter filterL_, filterR_;
    float filterFreqL_ = 900.0f, filterFreqR_ = 900.0f; // tolerance-skewed base cutoffs
    EffectorModule effector_;
    Smoother masterSm_;
};

} // namespace s42
