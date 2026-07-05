#pragma once

#include "engine/keyboard/Arpeggiator.h"
#include "engine/keyboard/KbClock.h"
#include "engine/keyboard/KbConfig.h"
#include "engine/keyboard/KbSequencer.h"
#include "engine/keyboard/PressureShaper.h"

// The touch keyboard's firmware (manual pp13-20) as pure, unit-testable
// logic — no JUCE, no jack registry. The Rack adapts it to the VoltBus:
// inputs are the CLOCK/RESET jack buffers plus the UI's touch gestures,
// outputs are the V/OCT, GATE L, GATE R and PRESSURE jack buffers.
//
// Behaviour routing (manual p14):
//   single -> V/OCT + GATE L carry the (12-plate) keyboard, GATE R mirrors
//             GATE L, PRESSURE carries the pressure shaper;
//   twin/split -> plates split 6/6, PRESSURE becomes the right side's V/oct
//             and GATE R its gate. Twin shares side[0]'s parameters, split
//             reads each side's own.
namespace s42 {

class TouchKeyboard
{
public:
    static constexpr float kGateVolts = 10.0f;
    static constexpr float kTouchThreshold = 0.02f;

    void prepare(double sampleRate) noexcept;
    void setConfig(const KbConfig& c) noexcept;

    void process(const KbTouch& touch, const float* clockJack, bool clockPatched,
                 const float* resetJack, float* voct, float* gateL, float* gateR,
                 float* press, int n) noexcept;

    // Telemetry / display state.
    uint16_t heldMask() const noexcept;              // global plate bits (held + arp latch)
    int stepOf(int side) const noexcept;             // arp/seq position, -1 in keyboard mode
    bool usingExternalClock() const noexcept { return clock_.external; }
    int octaveShift() const noexcept { return octave_; }
    bool offsetEnabled(int b) const noexcept { return offsetOn_[b & 1]; }
    float lastVoct(int side) const noexcept { return sides_[side & 1].glide; }

private:
    struct Side
    {
        uint16_t held = 0;    // local plate bits, from touch
        uint16_t latched = 0; // arp hold latch
        int order[12] = {};   // press order (local indices), last = priority
        int orderLen = 0;
        int activePlate = -1; // keyboard-mode sounding plate
        float plateV = kKbPlateBaseVolts; // last activated plate's designated volts
        float raw = kKbPlateBaseVolts;    // pre-quantise pitch incl. transposes
        float target = kKbPlateBaseVolts; // post-quantise/-offset pitch target
        float glide = kKbPlateBaseVolts;  // portamento output
        bool gate = false;
        float pulse = 0.0f;   // remaining gate-pulse samples (arp/seq)
        float vibEnv = 0.0f;
        float pressure = 0.0f; // max plate pressure on this side, 0..1
        Arpeggiator arp;
        KbSequencer seq;
        KbClockScaler scaler;
        float seqCv = 0.0f;   // held step CV (gated CV-update mode)
    };

    const KbSideConfig& sideCfg(int s) const noexcept
    {
        return config_.side[config_.behaviour == KbBehaviour::Split ? s : 0];
    }
    int numSides() const noexcept { return config_.behaviour == KbBehaviour::Single ? 1 : 2; }
    int plateCount() const noexcept { return config_.behaviour == KbBehaviour::Single ? 12 : 6; }
    int globalPlate(int side, int local) const noexcept
    {
        return config_.behaviour == KbBehaviour::Single ? local : side * 6 + local;
    }

    float sidePitch(int side, float rawVolts) const noexcept;
    void setNote(Side& sd, float rawVolts, bool allowGlide) noexcept;
    void updateTouch(const KbTouch& touch) noexcept;

    KbConfig config_ {};
    KbBehaviour lastBehaviour_ = KbBehaviour::Single;
    double sr_ = 48000.0;

    Side sides_[2];
    KbClock clock_;
    PressureShaper shaper_;

    KbTouch prevTouch_ {};
    bool prevButton_[2] = {};
    int octave_ = 0;          // single+quantiser transpose state, -3..+4
    bool offsetOn_[2] = {};   // offset toggles (single unquantised / twin / split)
    bool lastResetHigh_ = false;
    float vibPhase_ = 0.0f;
    bool anyTouch_ = false;   // pressure-shaper gate (block rate)
    bool touchRose_ = false;

    // Derived per process() call.
    float glideCoeff_ = 1.0f;
    float vibInc_ = 0.0f;
    float vibDepthV_ = 0.0f;
    float vibDelaySec_ = 0.0f;
    float riseSec_ = 0.0f, fallSec_ = 0.0f;
};

} // namespace s42
