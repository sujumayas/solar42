#include "fv1/CartridgeLibrary.h"
#include "fv1/SpinAsm.h"

#include <cassert>

namespace s42::fv1 {

#include "CartridgePrograms.inc"

static_assert(sizeof(kEntries) / sizeof(kEntries[0])
                  == CartridgeLibrary::kCartridges * CartridgeLibrary::kProgramsPerCartridge,
              "cartridge table must hold 4 cartridges x 3 programs");

namespace
{
int slot(int cartridge, int program) noexcept
{
    cartridge = cartridge < 0 ? 0
                              : cartridge >= CartridgeLibrary::kCartridges
                                    ? CartridgeLibrary::kCartridges - 1
                                    : cartridge;
    program = program < 0 ? 0
                          : program >= CartridgeLibrary::kProgramsPerCartridge
                                ? CartridgeLibrary::kProgramsPerCartridge - 1
                                : program;
    return cartridge * CartridgeLibrary::kProgramsPerCartridge + program;
}

struct AssembledTable
{
    std::array<std::array<uint32_t, Fv1Vm::kProgramWords>, 12> images {};

    AssembledTable()
    {
        for (int i = 0; i < 12; ++i)
        {
            const auto res = assembleSpinAsm(kEntries[i].source);
            // Sources are embedded at build time and covered by unit tests;
            // failure here means a broken build, not a runtime condition.
            assert(res.ok());
            images[(size_t) i] = res.words;
        }
    }
};
} // namespace

const CartridgeLibrary::Entry& CartridgeLibrary::entry(int cartridge, int program) noexcept
{
    return kEntries[slot(cartridge, program)];
}

const std::array<uint32_t, Fv1Vm::kProgramWords>& CartridgeLibrary::words(int cartridge,
                                                                          int program) noexcept
{
    static const AssembledTable table;
    return table.images[(size_t) slot(cartridge, program)];
}

} // namespace s42::fv1
