#include "fv1/Fv1Vm.h"
#include "fv1/Fv1Opcodes.h"

#include <algorithm>
#include <cmath>

namespace s42::fv1 {

namespace
{
constexpr int32_t kAccMax = 0x7FFFFF;  // S.23 rails
constexpr int32_t kAccMin = -0x800000;

inline int32_t sat24(int64_t v) noexcept
{
    return (int32_t) std::clamp<int64_t>(v, kAccMin, kAccMax);
}

inline int32_t signExtend24(uint32_t bits) noexcept
{
    return (int32_t) (bits << 8) >> 8;
}

// Raw coefficient fields, sign-extended to their real widths.
inline int64_t c16(uint32_t w) noexcept { return (int16_t) ((w >> 16) & 0xFFFF); }              // S1.14
inline int64_t c11(uint32_t w) noexcept { return ((int32_t) ((w >> 21) & 0x7FF) << 21) >> 21; } // S1.9
inline int64_t d11(uint32_t w) noexcept { return ((int32_t) ((w >> 5) & 0x7FF) << 21) >> 21; }  // S.10

inline int32_t floatToS23(float v) noexcept
{
    return sat24((int64_t) std::lround((double) v * 8388608.0));
}
inline float s23ToFloat(int32_t v) noexcept
{
    return (float) v * (1.0f / 8388608.0f);
}

// Ramp NA crossfade: 0 at the wrap, saturated-triangle 1 across the middle
// (rise over the first quarter, flat, fall over the last — AN-0001 fig. p7).
inline double naCrossfade(double pos01) noexcept
{
    return std::clamp((0.5 - std::abs(pos01 - 0.5)) * 4.0, 0.0, 1.0);
}
} // namespace

void Fv1Vm::loadProgram(const std::array<uint32_t, kProgramWords>& words) noexcept
{
    program_ = words;
    hasProgram_ = true;
    clearState();
}

void Fv1Vm::clearState() noexcept
{
    delay_.fill(0);
    regs_.fill(0);
    acc_ = pacc_ = lr_ = 0;
    dp_ = 0;
    ranOnce_ = false;
    for (int n = 0; n < 2; ++n)
    {
        sinPhase_[n] = sinLatch_[n] = 0.0;
        rmpPos_[n] = rmpLatch_[n] = 0.0;
        rmpRange_[n] = 4096;
    }
}

void Fv1Vm::setPot(int index, float value01) noexcept
{
    if (index >= 0 && index < 3)
        pots_[(size_t) index] = std::clamp(value01, 0.0f, 1.0f);
}

int32_t Fv1Vm::readDelay(int addr) noexcept
{
    lr_ = delay_[(size_t) ((dp_ + addr) & (kDelayWords - 1))];
    return lr_;
}

void Fv1Vm::writeDelay(int addr, int32_t v) noexcept
{
    delay_[(size_t) ((dp_ + addr) & (kDelayWords - 1))] = v;
}

void Fv1Vm::updateLfos() noexcept
{
    // Rates/ranges are re-read from the control registers every sample, so
    // programs can WRAX them live (AN-0001 ACC bit mappings; the hardware
    // truncates to the documented widths -> audible stepping is authentic).
    for (int n = 0; n < 2; ++n)
    {
        const uint32_t rate9 = ((uint32_t) regs_[(size_t) (reg::Sin0Rate + 2 * n)] >> 14) & 0x1FF;
        sinPhase_[n] += (double) rate9 / 131072.0; // Kf / 2^17 rad/sample
        if (sinPhase_[n] > 6.283185307179586)
            sinPhase_[n] -= 6.283185307179586;

        const auto rate16 = (int16_t) (((uint32_t) regs_[(size_t) (reg::Rmp0Rate + 2 * n)] >> 8) & 0xFFFF);
        const double range = (double) rmpRange_[n];
        // Positive rate = pitch UP (AN-0001): the read pointer (= sample age)
        // must shrink, so the position decrements by C/16384 per sample.
        rmpPos_[n] -= (double) rate16 / 16384.0;
        while (rmpPos_[n] >= range)
            rmpPos_[n] -= range;
        while (rmpPos_[n] < 0.0)
            rmpPos_[n] += range;
    }
}

void Fv1Vm::stepSampleCounter() noexcept
{
    dp_ = (dp_ - 1) & (kDelayWords - 1);
    updateLfos();
    ranOnce_ = true;
}

void Fv1Vm::execCho(uint32_t w) noexcept
{
    const uint32_t type = (w >> 30) & 0x3;
    const uint32_t flags = (w >> 24) & 0x3F;
    const int lfo = (int) ((w >> 21) & 0x3);
    const int addr = (int) ((w >> 5) & 0xFFFF);

    const bool isRamp = (lfo & 0x2) != 0;
    const int n = lfo & 0x1;

    double offsetSamples = 0.0; // LFO address contribution
    double coeff = 0.0;         // interpolation / crossfade coefficient
    double rdalValue = 0.0;     // S.23-domain value for CHO RDAL

    if (! isRamp)
    {
        if (flags & cho::Reg)
            sinLatch_[n] = sinPhase_[n];
        const double phase = sinLatch_[n];
        const double amp = (double) (((uint32_t) regs_[(size_t) (reg::Sin0Range + 2 * n)] >> 8) & 0x7FFF);
        double wave = (flags & cho::Cos) ? std::cos(phase) : std::sin(phase);
        if (flags & cho::Compa)
            wave = -wave;
        const double pos = wave * amp * 0.25; // sweep = +-Ka/4 samples (AN-0001)
        offsetSamples = pos;
        const double fl = std::floor(pos);
        coeff = pos - fl;
        rdalValue = wave * amp * 256.0; // range reg units: full Ka -> full scale
    }
    else
    {
        if (flags & cho::Reg)
            rmpLatch_[n] = rmpPos_[n];
        const double range = (double) rmpRange_[n];
        double pos = rmpLatch_[n];
        if (flags & cho::Rptr2)
        {
            pos += range * 0.5;
            if (pos >= range)
                pos -= range;
        }
        if (flags & cho::Compa)
        {
            pos = range - pos;
            if (pos >= range)
                pos -= range;
        }
        offsetSamples = pos;
        const double fl = std::floor(pos);
        coeff = pos - fl;
        if (flags & cho::Na)
            coeff = naCrossfade(rmpLatch_[n] / range);
        rdalValue = pos * 256.0; // pointer units, same scale as ADDR_PTR
    }

    if (flags & cho::Compc)
        coeff = 1.0 - coeff;

    // The interpolation coefficient is 14 bits wide on the chip.
    const int64_t coeff14 = std::clamp<int64_t>((int64_t) std::floor(coeff * 16384.0), 0, 16384);

    const int32_t oldAcc = acc_;
    switch (type)
    {
        case cho::TypeRda:
        {
            // NA reads the plain instruction address (crossfade-only mode).
            const int ea = (flags & cho::Na) ? addr
                                             : addr + (int) std::floor(offsetSamples);
            const int32_t s = readDelay(ea);
            acc_ = sat24((int64_t) acc_ + (((int64_t) s * coeff14) >> 14));
            break;
        }
        case cho::TypeSof:
        {
            const int64_t d = (int64_t) ((int16_t) addr) << 8; // S.15 -> S.23
            acc_ = sat24((((int64_t) acc_ * coeff14) >> 14) + d);
            break;
        }
        case cho::TypeRdal:
        {
            acc_ = sat24((int64_t) std::llround(rdalValue));
            break;
        }
        default: break;
    }
    pacc_ = oldAcc;
}

void Fv1Vm::runProgram() noexcept
{
    int pc = 0;
    while (pc < kProgramWords)
    {
        const uint32_t w = program_[(size_t) pc];
        ++pc;

        const auto op = (Op) (w & 0x1F);
        const int regA = (int) ((w >> 5) & 0x3F);
        const int addr15 = (int) ((w >> 5) & 0x7FFF);
        const int32_t oldAcc = acc_;

        switch (op)
        {
            case Op::Rda:
                acc_ = sat24((int64_t) acc_ + (((int64_t) readDelay(addr15) * c11(w)) >> 9));
                break;
            case Op::Rmpa:
            {
                const int ea = (int) (((uint32_t) regs_[reg::AddrPtr] >> 8) & 0xFFFF);
                acc_ = sat24((int64_t) acc_ + (((int64_t) readDelay(ea) * c11(w)) >> 9));
                break;
            }
            case Op::Wra:
                writeDelay(addr15, acc_);
                acc_ = sat24(((int64_t) acc_ * c11(w)) >> 9);
                break;
            case Op::Wrap:
                writeDelay(addr15, acc_);
                acc_ = sat24((((int64_t) acc_ * c11(w)) >> 9) + lr_);
                break;
            case Op::Rdax:
                acc_ = sat24((int64_t) acc_ + (((int64_t) regs_[(size_t) regA] * c16(w)) >> 14));
                break;
            case Op::Rdfx:
            {
                const int64_t r = regs_[(size_t) regA];
                acc_ = sat24(((((int64_t) acc_ - r) * c16(w)) >> 14) + r);
                break;
            }
            case Op::Wrax:
                regs_[(size_t) regA] = acc_;
                acc_ = sat24(((int64_t) acc_ * c16(w)) >> 14);
                break;
            case Op::Wrhx:
                regs_[(size_t) regA] = acc_;
                acc_ = sat24((((int64_t) acc_ * c16(w)) >> 14) + pacc_);
                break;
            case Op::Wrlx:
                regs_[(size_t) regA] = acc_;
                acc_ = sat24(((((int64_t) pacc_ - acc_) * c16(w)) >> 14) + pacc_);
                break;
            case Op::Maxx:
            {
                const int64_t a = std::abs((int64_t) regs_[(size_t) regA] * c16(w)) >> 14;
                acc_ = sat24(std::max<int64_t>(a, std::abs((int64_t) acc_)));
                break;
            }
            case Op::Mulx:
            {
                const int64_t creg = regs_[(size_t) regA] >> 9; // top 15 bits = S.14
                acc_ = sat24(((int64_t) acc_ * creg) >> 14);
                break;
            }
            case Op::Log:
            {
                // Result lives in S4.19: value = log2(|acc|)/16, saturated to S.23.
                const double a = std::abs((double) acc_) / 8388608.0;
                const double y = a > 0.0 ? std::log2(a) / 16.0 : -1.0;
                const int64_t yS23 = std::clamp<int64_t>((int64_t) std::llround(y * 8388608.0),
                                                         kAccMin, kAccMax);
                acc_ = sat24(((yS23 * c16(w)) >> 14) + (d11(w) << 13));
                break;
            }
            case Op::Exp:
            {
                const double x = (double) acc_ / 8388608.0 * 16.0; // S4.19 exponent
                const int64_t rS23 = x >= 0.0 ? kAccMax
                                              : (int64_t) std::llround(std::exp2(x) * 8388608.0);
                acc_ = sat24(((rS23 * c16(w)) >> 14) + (d11(w) << 13));
                break;
            }
            case Op::Sof:
                acc_ = sat24((((int64_t) acc_ * c16(w)) >> 14) + (d11(w) << 13));
                break;
            case Op::And:
                acc_ = signExtend24(((uint32_t) acc_ & 0xFFFFFF) & ((w >> 8) & 0xFFFFFF));
                break;
            case Op::Or:
                acc_ = signExtend24(((uint32_t) acc_ & 0xFFFFFF) | ((w >> 8) & 0xFFFFFF));
                break;
            case Op::Xor:
                acc_ = signExtend24(((uint32_t) acc_ & 0xFFFFFF) ^ ((w >> 8) & 0xFFFFFF));
                break;
            case Op::Skp:
            {
                // All selected conditions must hold; an empty mask is
                // vacuously true (asfv1's JMP pseudo-op = SKP 0,target).
                const uint32_t cond = (w >> 27) & 0x1F;
                const int nSkip = (int) ((w >> 21) & 0x3F);
                bool takeIt = true;
                if ((cond & skp::Neg) && ! (acc_ < 0)) takeIt = false;
                if ((cond & skp::Gez) && ! (acc_ >= 0)) takeIt = false;
                if ((cond & skp::Zro) && ! (acc_ == 0)) takeIt = false;
                if ((cond & skp::Zrc) && ! ((acc_ ^ pacc_) < 0)) takeIt = false;
                if ((cond & skp::Run) && ! ranOnce_) takeIt = false;
                if (takeIt)
                    pc += nSkip;
                break;
            }
            case Op::Wlds: // also WLDR: bit 30 selects the ramp form
            {
                if ((w >> 30) & 0x1)
                {
                    const int n = (int) ((w >> 29) & 0x1);
                    const auto f = (int16_t) ((w >> 13) & 0xFFFF);
                    static constexpr int kRanges[] = { 4096, 2048, 1024, 512 };
                    rmpRange_[n] = kRanges[(w >> 5) & 0x3];
                    regs_[(size_t) (reg::Rmp0Rate + 2 * n)] = (int32_t) f << 8;
                    regs_[(size_t) (reg::Rmp0Range + 2 * n)] = rmpRange_[n] << 8;
                    rmpPos_[n] = 0.0;
                }
                else
                {
                    const int n = (int) ((w >> 29) & 0x1);
                    const uint32_t f = (w >> 20) & 0x1FF;
                    const uint32_t a = (w >> 5) & 0x7FFF;
                    regs_[(size_t) (reg::Sin0Rate + 2 * n)] = (int32_t) (f << 14);
                    regs_[(size_t) (reg::Sin0Range + 2 * n)] = (int32_t) (a << 8);
                    sinPhase_[n] = 0.0;
                }
                break;
            }
            case Op::Jam:
                rmpPos_[(w >> 6) & 0x1] = 0.0;
                break;
            case Op::Cho:
                execCho(w);
                continue; // CHO manages PACC itself
            default:
                break;
        }
        pacc_ = oldAcc;
    }
}

void Fv1Vm::processSample(float inL, float inR, float& outL, float& outR) noexcept
{
    if (! hasProgram_)
    {
        outL = inL;
        outR = inR;
        return;
    }

    // POT ADCs: 9-bit deliberate quantization (with hysteresis on hardware).
    for (int p = 0; p < 3; ++p)
    {
        float v = pots_[(size_t) p];
        if (potQuantize_)
            v = std::floor(v * 511.0f) * (1.0f / 511.0f);
        regs_[(size_t) (reg::Pot0 + p)] = floatToS23(v * 0.9999999f);
    }

    regs_[reg::AdcL] = floatToS23(inL);
    regs_[reg::AdcR] = floatToS23(inR);

    runProgram();

    outL = s23ToFloat(regs_[reg::DacL]);
    outR = s23ToFloat(regs_[reg::DacR]);

    stepSampleCounter();
}

} // namespace s42::fv1
