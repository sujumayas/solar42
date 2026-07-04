#include "fv1/Fv1Vm.h"
#include "dsp/Volts.h"

namespace s42::fv1 {

void Fv1Vm::loadProgram(const std::array<uint32_t, kProgramWords>& words) noexcept
{
    program_ = words;
    hasProgram_ = true;
    clearState();
}

void Fv1Vm::clearState() noexcept
{
    // M4: wipe delay RAM, JAM ramp LFOs, zero registers.
}

void Fv1Vm::setPot(int index, float value01) noexcept
{
    if (index >= 0 && index < 3)
        pots_[(size_t) index] = clamp01(value01);
}

void Fv1Vm::processSample(float inL, float inR, float& outL, float& outR) noexcept
{
    // M4: interpreter. Until then the effector is a transparent wire.
    outL = inL;
    outR = inR;
}

} // namespace s42::fv1
