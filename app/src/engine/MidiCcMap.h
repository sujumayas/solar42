#pragma once

#include <atomic>
#include <cstdint>

// Right-click CC learn (M9c P5). The audio thread pushes every incoming
// controller event into a lock-free SPSC ring; the message thread drains it
// at timer rate and either consumes the first event as a LEARN binding (one
// CC per param — re-learning moves the binding; binding a CC that already
// drove another param retargets it) or dispatches value01 to the bound
// parameter slot. Slots are opaque ints to stay JUCE-free (the processor
// uses parameter indices and persists bindings by parameter ID).
//
// CC 64 (sustain, owned by the MidiPerformer) and the channel-mode group
// 120..127 are reserved — never learnable, never dispatched.
namespace s42 {

class MidiCcMap
{
public:
    static constexpr int kNumCc = 128;
    static constexpr int kUnmapped = -1;

    MidiCcMap() noexcept { clearAll(); }

    static constexpr bool learnable(int cc) noexcept
    {
        return cc >= 0 && cc < 120 && cc != 64;
    }

    // ---- audio thread: raw controller events (any channel). Full ring
    // drops the event — the next twitch of the knob follows right behind.
    void onCcFromAudio(int cc, int value7) noexcept
    {
        if (cc < 0 || cc >= kNumCc)
            return;
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire))
            return;
        ring_[head] = { (uint8_t) cc, (uint8_t) (value7 & 0x7F) };
        head_.store(next, std::memory_order_release);
    }

    // ---- message thread only, below here.
    void armLearn(int slot) noexcept { armed_ = slot; }
    void cancelLearn() noexcept { armed_ = kUnmapped; }
    int armedSlot() const noexcept { return armed_; }

    void bind(int cc, int slot) noexcept
    {
        if (!learnable(cc) || slot < 0)
            return;
        clearSlot(slot); // one CC per param
        map_[cc] = (int16_t) slot;
    }

    void clearSlot(int slot) noexcept
    {
        for (auto& m : map_)
            if (m == slot)
                m = kUnmapped;
    }

    void clearAll() noexcept
    {
        for (auto& m : map_)
            m = kUnmapped;
    }

    int slotForCc(int cc) const noexcept
    {
        return cc >= 0 && cc < kNumCc ? map_[cc] : kUnmapped;
    }

    int ccForSlot(int slot) const noexcept
    {
        if (slot >= 0)
            for (int cc = 0; cc < kNumCc; ++cc)
                if (map_[cc] == slot)
                    return cc;
        return kUnmapped;
    }

    // Drain pending events. While learn is armed the first learnable event
    // binds (and is consumed — the gesture that names the knob shouldn't
    // also yank the value); everything else bound dispatches through
    // fn(slot, value01).
    template <typename Fn>
    void drain(Fn&& fn)
    {
        Event e;
        while (pop(e))
        {
            if (!learnable(e.cc))
                continue;
            if (armed_ != kUnmapped)
            {
                bind(e.cc, armed_);
                armed_ = kUnmapped;
                continue;
            }
            if (map_[e.cc] != kUnmapped)
                fn((int) map_[e.cc], (float) e.value * (1.0f / 127.0f));
        }
    }

private:
    struct Event
    {
        uint8_t cc, value;
    };

    bool pop(Event& e) noexcept
    {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false;
        e = ring_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    static constexpr uint32_t kSize = 256, kMask = kSize - 1;
    Event ring_[kSize] {};
    std::atomic<uint32_t> head_ { 0 }, tail_ { 0 };

    int16_t map_[kNumCc] = {}; // clearAll() in the ctor sets kUnmapped
    int armed_ = kUnmapped;
};

} // namespace s42
