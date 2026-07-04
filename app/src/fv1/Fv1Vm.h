#pragma once

#include <array>
#include <cstdint>

namespace s42::fv1 {

// Spin FV-1 virtual machine — fixed-point core (M4).
// Chip facts: 32,768 Hz clock, 128-instruction programs, 32768-word delay RAM,
// ACC/REG0-31/POT0-2/ADC/DAC in S.23, coefficients S1.14/S1.9/S.10,
// SIN0/1 + RMP0/1 LFOs, auto-decrementing delay pointer.
//
// M0: interface skeleton only, so the engine/library shape is fixed early.
// The interpreter, ISA decode, and LFO/CHO semantics land in M4.
class Fv1Vm
{
public:
    static constexpr int kProgramWords = 128;
    static constexpr int kDelayWords = 32768;
    static constexpr double kClockHz = 32768.0;

    void loadProgram(const std::array<uint32_t, kProgramWords>& words) noexcept;
    void clearState() noexcept; // delay RAM wipe + LFO JAM (cartridge program load)

    void setPot(int index, float value01) noexcept;

    // One chip sample: audio in/out at the VM's native 32,768 Hz rate.
    void processSample(float inL, float inR, float& outL, float& outR) noexcept;

private:
    std::array<uint32_t, kProgramWords> program_ {};
    std::array<float, 3> pots_ {};
    bool hasProgram_ = false;
};

} // namespace s42::fv1
