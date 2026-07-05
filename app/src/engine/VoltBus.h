#pragma once

#include <array>
#include <cstring>

#include "engine/Jacks.h"

namespace s42 {

// One 64-sample buffer per outlet, all values in volts. Modules run in the
// Rack's fixed order and write their outlets in place; a cable pointing
// "backwards" in that order therefore reads the previous sub-block's data
// (~1.3 ms loop delay @48k) — exactly the hardware-feedback semantics we
// want, with no double buffering.
//
// Patch edits arrive on the audio thread as commands (see CommandQueue) and
// mutate the resolved source table between sub-blocks.
class VoltBus
{
public:
    static constexpr int kSubBlock = 64;
    static constexpr int16_t kNoSource = -1;

    VoltBus() { clearAll(); }

    void clearAll() noexcept
    {
        patched_.fill(kNoSource);
        clearBuffers();
    }

    // Zero the signal state WITHOUT unplugging cables — prepare() re-inits
    // DSP through this: power-cycling the rack must not pull patch cords.
    void clearBuffers() noexcept
    {
        for (auto& b : buffers_)
            b.fill(0.0f);
        zero_.fill(0.0f);
        resolveAll();
    }

    float* out(Outlet o) noexcept { return buffers_[(size_t) o].data(); }
    const float* outRead(Outlet o) const noexcept { return buffers_[(size_t) o].data(); }

    // The buffer an inlet reads this sub-block (never null; zeros when open).
    const float* in(Inlet i) const noexcept
    {
        const int16_t src = sourceOf_[(size_t) i];
        return src == kNoSource ? zero_.data() : buffers_[(size_t) src].data();
    }

    // True when a cable is physically in the jack (breaks internal normals —
    // and models jack-presence switches like the sequencer's ext clock).
    bool isPatched(Inlet i) const noexcept { return patched_[(size_t) i] != kNoSource; }

    // True when the inlet reads *something* (cable or normal).
    bool isConnected(Inlet i) const noexcept { return sourceOf_[(size_t) i] != kNoSource; }

    void patch(Inlet i, Outlet o) noexcept
    {
        patched_[(size_t) i] = (int16_t) o;
        resolveAll();
    }

    void unpatch(Inlet i) noexcept
    {
        patched_[(size_t) i] = kNoSource;
        resolveAll();
    }

private:
    void resolveAll() noexcept
    {
        for (int i = 0; i < kInletCount; ++i)
            sourceOf_[(size_t) i] = resolve((Inlet) i);
    }

    int16_t resolve(Inlet i) const noexcept
    {
        const int16_t cable = patched_[(size_t) i];
        if (cable != kNoSource)
            return cable;

        const auto& info = kInletInfo[(size_t) i];
        switch (info.kind)
        {
            case NormalKind::FromOutlet: return (int16_t) info.normalOutlet;
            case NormalKind::FollowCvL:  return patched_[(size_t) Inlet::FiltCvLIn];
            case NormalKind::None:
            case NormalKind::Internal:   // module supplies its own fallback
            case NormalKind::HostInput:  // rack supplies the host input
                return kNoSource;
        }
        return kNoSource;
    }

    std::array<std::array<float, kSubBlock>, (size_t) kOutletCount> buffers_ {};
    std::array<float, kSubBlock> zero_ {};
    std::array<int16_t, (size_t) kInletCount> patched_ {};
    std::array<int16_t, (size_t) kInletCount> sourceOf_ {};
};

} // namespace s42
