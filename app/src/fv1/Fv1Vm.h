#pragma once

#include <array>
#include <cstdint>

namespace s42::fv1 {

// Spin FV-1 virtual machine — fixed-point core.
//
// Chip facts (datasheet + Spin knowledge base + AN-0001, fetched 2026-07-04):
// 32,768 Hz clock, 128-instruction programs, 32768-word delay RAM with an
// auto-decrementing base pointer, ACC/REG0-31/POT/ADC/DAC in 24-bit S.23
// two's complement with saturation, coefficients S1.14 / S1.9 / S.10 decoded
// at real widths from the 32-bit words, POTs quantized to 9 bits with
// hysteresis, SIN0/1 + RMP0/1 LFOs driving the CHO instruction.
//
// LFO scaling (AN-0001):
//   sin:  phase += Kf / 2^17 rad/sample, Kf = rate reg [22:14] (9 bits)
//         address sweep = +-Ka/4 samples, Ka = range reg [22:8] (15 bits)
//   ramp: pointer speed = C / 16384 samples/sample, C = rate reg [23:8]
//         (signed 16 bits); range 4096/2048/1024/512; NA crossfade is the
//         saturated triangle 0->1 over the first quarter, flat, 1->0 last.
//
// The delay RAM is stored at full S.23 here (the real chip compresses it to
// a small float "for economy" — format undocumented, deliberately skipped).
class Fv1Vm
{
public:
    static constexpr int kProgramWords = 128;
    static constexpr int kDelayWords = 32768;
    static constexpr double kClockHz = 32768.0;

    void loadProgram(const std::array<uint32_t, kProgramWords>& words) noexcept;
    void clearState() noexcept; // delay RAM wipe + LFO JAM (cartridge program load)

    void setPot(int index, float value01) noexcept;
    void setPotQuantize(bool on) noexcept { potQuantize_ = on; } // 9-bit ADC character

    // One chip sample: audio in/out at the VM's native 32,768 Hz rate.
    void processSample(float inL, float inR, float& outL, float& outR) noexcept;

    bool hasProgram() const noexcept { return hasProgram_; }

    // Test/debug access.
    int32_t acc() const noexcept { return acc_; }
    int32_t regValue(int address) const noexcept { return regs_[(size_t) (address & 0x3F)]; }
    void setAcc(int32_t v) noexcept { acc_ = v; }
    void setReg(int address, int32_t v) noexcept { regs_[(size_t) (address & 0x3F)] = v; }
    int32_t delayAt(int addr) const noexcept { return delay_[(size_t) ((dp_ + addr) & (kDelayWords - 1))]; }
    void runProgram() noexcept; // execute the 128 words once (no ADC/DAC/pointer step)
    void stepSampleCounter() noexcept;

private:
    void updateLfos() noexcept;
    void execCho(uint32_t w) noexcept;
    int32_t readDelay(int addr) noexcept;
    void writeDelay(int addr, int32_t v) noexcept;

    std::array<uint32_t, kProgramWords> program_ {};
    bool hasProgram_ = false;

    std::array<int32_t, kDelayWords> delay_ {};
    std::array<int32_t, 64> regs_ {};
    int32_t acc_ = 0, pacc_ = 0, lr_ = 0;
    int dp_ = 0;             // down-counting delay base pointer
    bool ranOnce_ = false;   // SKP RUN flag

    std::array<float, 3> pots_ {};
    bool potQuantize_ = true;

    // SIN/COS LFOs (phase in radians) and RAMP LFOs (position in samples).
    double sinPhase_[2] = { 0.0, 0.0 };
    double rmpPos_[2] = { 0.0, 0.0 };
    int rmpRange_[2] = { 4096, 4096 };
    // CHO REG latches: snapshot of the LFO state taken by the REG flag.
    double sinLatch_[2] = { 0.0, 0.0 };   // latched phase
    double rmpLatch_[2] = { 0.0, 0.0 };   // latched position (samples)
};

} // namespace s42::fv1
