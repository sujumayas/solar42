#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace s42 {

// Lock-free SPSC ring carrying patch edits from the message thread to the
// audio thread. Commands are tiny PODs; the audio thread drains between
// sub-blocks. Capacity 256 — a UI can't produce edits faster than that
// drains, and push() reports (and the UI retries) if it ever fills.
struct PatchCmd
{
    int16_t inlet;  // Inlet index
    int16_t outlet; // Outlet index, or -1 = unpatch
};

class CommandQueue
{
public:
    bool push(PatchCmd c) noexcept
    {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire))
            return false; // full
        ring_[head] = c;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(PatchCmd& c) noexcept
    {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false; // empty
        c = ring_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

private:
    static constexpr uint32_t kSize = 256, kMask = kSize - 1;
    std::array<PatchCmd, kSize> ring_ {};
    std::atomic<uint32_t> head_ { 0 }, tail_ { 0 };
};

} // namespace s42
