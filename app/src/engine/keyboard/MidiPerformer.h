#pragma once

#include <atomic>
#include <cstdint>

#include "engine/keyboard/KbConfig.h"

// MIDI-in performance adapter (M9a — spec locked with the user 2026-07-05,
// see 08-implementation-plan.md "M9 breakdown"). Translates held MIDI notes
// into the exact gestures the panel produces — KbTouch plate pressures plus
// the digital-only per-plate octave shift, and the six DRONE VOICES gates —
// so everything downstream (quantiser, microtuning, twin/split, arp, seq,
// pressure modes, drone envelopes) cannot tell a keyboard from a finger.
//
//   notes 24-29 (C1-F1)  drone voices 1-6: momentary gate, or HOLD-latch
//                        toggle when latch mode is on (persisted checkbox)
//   notes 30-35          guard band, ignored
//   notes 36+            the 12 plates, plate = note mod 12 (C = plate 1);
//                        60-71 (C4-B4) = native plate tuning, each keyboard
//                        octave away shifts that touch +-1 V — beyond the
//                        hardware's reach, panel OCT state untouched
//   velocity             initial plate pressure (floored above the plate
//                        touch threshold so pp notes still speak)
//   channel / poly AT    raise pressure above the velocity while held
//   CC64 sustain         holds plates and (momentary mode) drone gates
//
// Audio-thread owned. The only cross-thread traffic is the HOLD-toggle
// counters: the processor's message-thread timer turns odd deltas into
// dN.hold parameter flips, so the panel HOLD buttons and MIDI share one
// latch state instead of fighting over two.
namespace s42 {

class MidiPerformer
{
public:
    static constexpr int kDroneFirst = 24, kDroneLast = 29; // C1..F1
    static constexpr int kPlateFirst = 36;                  // C2
    static constexpr int kNativeOct = 60 / 12;              // C4 octave = native
    static constexpr float kMinPressure = 0.05f;            // > kTouchThreshold

    void setLatch(bool on) noexcept { latch_ = on; }

    void noteOn(int note, float vel01) noexcept
    {
        if (note >= kDroneFirst && note <= kDroneLast)
        {
            const int v = note - kDroneFirst;
            if (latch_)
                toggles_[v].fetch_add(1, std::memory_order_relaxed);
            else
                droneDown_[v] = droneRing_[v] = true;
            return;
        }
        if (note < kPlateFirst || note > 127)
            return;
        Note& n = notes_[note];
        n.down = n.ringing = true;
        n.vel = vel01 < kMinPressure ? kMinPressure : vel01;
        n.polyAt = 0.0f;
        n.stamp = ++stamp_;
    }

    void noteOff(int note) noexcept
    {
        if (note >= kDroneFirst && note <= kDroneLast)
        {
            const int v = note - kDroneFirst;
            droneDown_[v] = false;
            if (!sustain_)
                droneRing_[v] = false;
            return;
        }
        if (note < kPlateFirst || note > 127)
            return;
        notes_[note].down = false;
        if (!sustain_)
            notes_[note].ringing = false;
    }

    void channelPressure(float v01) noexcept { chanAt_ = v01; }

    void polyPressure(int note, float v01) noexcept
    {
        if (note >= kPlateFirst && note <= 127)
            notes_[note].polyAt = v01;
    }

    void sustain(bool down) noexcept
    {
        sustain_ = down;
        if (!down)
        {
            for (Note& n : notes_)
                n.ringing = n.ringing && n.down;
            for (int v = 0; v < 6; ++v)
                droneRing_[v] = droneRing_[v] && droneDown_[v];
        }
    }

    void reset() noexcept
    {
        for (Note& n : notes_)
            n = Note {};
        for (int v = 0; v < 6; ++v)
            droneDown_[v] = droneRing_[v] = false;
        chanAt_ = 0.0f;
        sustain_ = false;
    }

    // Merge into the UI's touch snapshot: pressures max together; a ringing
    // MIDI note owns its plate's octave shift (finger touches are always
    // shift 0, and of two MIDI octaves on one plate the last-pressed wins).
    // droneGate[] comes back in voice-number order 1..6, all false in latch
    // mode (latching rides the toggle counters instead).
    void apply(KbTouch& touch, bool droneGate[6]) const noexcept
    {
        uint32_t best[12] = {};
        for (int note = kPlateFirst; note < 128; ++note)
        {
            const Note& n = notes_[note];
            if (!n.ringing)
                continue;
            const int plate = note % 12;
            float p = n.vel;
            p = chanAt_ > p ? chanAt_ : p;
            p = n.polyAt > p ? n.polyAt : p;
            if (p > touch.plate[plate])
                touch.plate[plate] = p;
            if (n.stamp > best[plate])
            {
                best[plate] = n.stamp;
                touch.plateShift[plate] = (int8_t) (note / 12 - kNativeOct);
            }
        }
        for (int v = 0; v < 6; ++v)
            droneGate[v] = !latch_ && droneRing_[v];
    }

    // Message-thread side of latch mode: monotonic per-voice toggle counts.
    uint32_t toggles(int voice) const noexcept
    {
        return toggles_[voice].load(std::memory_order_relaxed);
    }

private:
    struct Note
    {
        bool down = false, ringing = false;
        float vel = 0.0f, polyAt = 0.0f;
        uint32_t stamp = 0;
    };

    Note notes_[128] {};
    bool droneDown_[6] = {}, droneRing_[6] = {};
    std::atomic<uint32_t> toggles_[6] {};
    float chanAt_ = 0.0f;
    bool sustain_ = false;
    bool latch_ = false;
    uint32_t stamp_ = 0;
};

} // namespace s42
