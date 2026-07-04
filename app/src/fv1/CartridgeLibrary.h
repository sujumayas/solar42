#pragma once

#include "fv1/Fv1Vm.h"

#include <array>

namespace s42::fv1 {

// The starter cartridge set (07 §5): CATHEDRAL, TIME, VIBROTREM, OCHRE —
// our own SpinASM approximations of the documented X/Y/Z catalog, embedded
// at build time from app/programs and assembled once on first use.
//
// Hardware slot semantics live in EffectorModule: flipping a channel's 1-2-3
// toggle loads that program from the currently inserted cartridge; removing
// or swapping the cartridge changes nothing until a toggle flips.
class CartridgeLibrary
{
public:
    static constexpr int kCartridges = 4;
    static constexpr int kProgramsPerCartridge = 3;

    struct Entry
    {
        const char* cartridge; // "CATHEDRAL"
        const char* program;   // "SHIMMER"
        const char* source;    // SpinASM text
    };

    static const Entry& entry(int cartridge, int program) noexcept;

    // Assembled 128-word images. First call assembles all twelve programs
    // (allocates); call ensureAssembled() from a prepare() path so the audio
    // thread only ever hits the cached table.
    static const std::array<uint32_t, Fv1Vm::kProgramWords>& words(int cartridge,
                                                                   int program) noexcept;
    static void ensureAssembled() noexcept { (void) words(0, 0); }

    static const char* cartridgeName(int cartridge) noexcept
    {
        return entry(cartridge, 0).cartridge;
    }
};

} // namespace s42::fv1
