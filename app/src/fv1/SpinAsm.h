#pragma once

#include "fv1/Fv1Vm.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace s42::fv1 {

// Runtime SpinASM assembler. Dialect and bit-exact output match asfv1 1.2.7
// (the cross-check reference; see tests/reference): labels, EQU/MEM (either
// order), `name#` end / `name^` midpoint delay modifiers, $/%/0x literals,
// | ^ & << >> + - * / ( ) ~ expressions, SKP label targets, LDAX/ABSA/CLR/
// NOT/NOP/JMP/RAW pseudo-ops, per-width real coefficient encoding with
// clamping, CHO flag masking per LFO type.
struct AsmResult
{
    std::array<uint32_t, Fv1Vm::kProgramWords> words {}; // padded with SKP 0,0
    int instructionCount = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool ok() const noexcept { return errors.empty(); }
};

AsmResult assembleSpinAsm(std::string_view source);

} // namespace s42::fv1
