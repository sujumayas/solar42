#pragma once

#include <atomic>
#include <cstdint>

namespace s42 {

// Single-writer seqlock for handing a POD snapshot between threads (same
// scheme as Telemetry, generic): the writer never blocks, the reader retries
// and keeps its last good copy on contention. Used for the keyboard config
// (message thread writes, audio thread reads).
template <typename T>
class SeqlockBox
{
public:
    void write(const T& v) noexcept
    {
        const uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_release); // odd = write in progress
        std::atomic_thread_fence(std::memory_order_release);
        data_ = v;
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s + 2, std::memory_order_release);
    }

    // False when every retry hit a concurrent write (caller keeps last good).
    bool read(T& out) const noexcept
    {
        for (int tries = 0; tries < 8; ++tries)
        {
            const uint32_t a = seq_.load(std::memory_order_acquire);
            if ((a & 1u) != 0)
                continue;
            std::atomic_thread_fence(std::memory_order_acquire);
            out = data_;
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_acquire) == a)
                return true;
        }
        return false;
    }

private:
    std::atomic<uint32_t> seq_ { 0 };
    T data_ {};
};

} // namespace s42
