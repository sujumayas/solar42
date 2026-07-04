#pragma once

#include <cstdint>

// Spin FV-1 instruction encodings — verified against asfv1 1.2.7's op table
// (the equivalence reference for our assembler) and the Spin knowledge-base
// "Instructions and Syntax" page. One 32-bit word per instruction, opcode in
// bits [4:0]. Shared by the VM decoder, the SpinASM assembler and the tests.
namespace s42::fv1 {

enum class Op : uint32_t
{
    Rda  = 0b00000, // acc += mem[addr] * C(S1.9);  addr 15b<<5, C 11b<<21
    Rmpa = 0b00001, // acc += mem[addr_ptr] * C;    C 11b<<21
    Wra  = 0b00010, // mem[addr] = acc; acc *= C
    Wrap = 0b00011, // mem[addr] = acc; acc = acc*C + LR
    Rdax = 0b00100, // acc += reg * C(S1.14);       reg 6b<<5, C 16b<<16
    Rdfx = 0b00101, // acc = (acc - reg)*C + reg    (LDAX = RDFX reg,0)
    Wrax = 0b00110, // reg = acc; acc *= C
    Wrhx = 0b00111, // reg = acc; acc = acc*C + PACC
    Wrlx = 0b01000, // reg = acc; acc = (PACC-acc)*C + PACC
    Maxx = 0b01001, // acc = max(|reg*C|, |acc|)    (ABSA = MAXX 0,0)
    Mulx = 0b01010, // acc *= reg (top 15 bits of reg as S.14 coefficient)
    Log  = 0b01011, // acc = C*log2(|acc|)/16 + D;  C 16b<<16, D 11b<<5
    Exp  = 0b01100, // acc = C*2^(acc*16) + D
    Sof  = 0b01101, // acc = C*acc + D(S.10)
    And  = 0b01110, // acc &= M;                    M 24b<<8  (CLR = AND 0)
    Or   = 0b01111, // acc |= M
    Xor  = 0b10000, // acc ^= M                     (NOT = XOR $FFFFFF)
    Skp  = 0b10001, // skip N if all flagged conditions true; cond 5b<<27, N 6b<<21
    Wlds = 0b10010, // sin LFO init:  n 1b<<29, F 9b<<20, A 15b<<5
    Wldr = 0b10010, // ramp LFO init: n 2b<<29 (10/11), F 16b<<13, A 2b<<5
    Jam  = 0b10011, // ramp LFO n reset; n 2b<<6 (10/11)
    Cho  = 0b10100, // type 2b<<30 (RDA=00, SOF=10, RDAL=11), flags 6b<<24,
                    // lfo 2b<<21, addr/D 16b<<5
};

// SKP condition flags (bits [31:27]); skip when ALL selected are true.
namespace skp
{
inline constexpr uint32_t Neg = 0x01; // ACC < 0
inline constexpr uint32_t Gez = 0x02; // ACC >= 0
inline constexpr uint32_t Zro = 0x04; // ACC == 0
inline constexpr uint32_t Zrc = 0x08; // sign(ACC) != sign(PACC)
inline constexpr uint32_t Run = 0x10; // not the first program execution
} // namespace skp

// CHO flags (bits [29:24]). SIN LFOs use bits 0-3, RMP LFOs bits 1-5.
namespace cho
{
inline constexpr uint32_t Sin   = 0x00;
inline constexpr uint32_t Cos   = 0x01; // COS output of a SIN/COS LFO
inline constexpr uint32_t Reg   = 0x02; // latch LFO outputs for later CHOs
inline constexpr uint32_t Compc = 0x04; // coefficient = 1 - frac
inline constexpr uint32_t Compa = 0x08; // complement the address offset
inline constexpr uint32_t Rptr2 = 0x10; // ramp pointer + half range
inline constexpr uint32_t Na    = 0x20; // ramp crossfade coeff, plain address
inline constexpr uint32_t TypeRda  = 0x0;
inline constexpr uint32_t TypeSof  = 0x2;
inline constexpr uint32_t TypeRdal = 0x3;
} // namespace cho

// Control-register addresses (the 6-bit register field).
namespace reg
{
inline constexpr int Sin0Rate  = 0x00;
inline constexpr int Sin0Range = 0x01;
inline constexpr int Sin1Rate  = 0x02;
inline constexpr int Sin1Range = 0x03;
inline constexpr int Rmp0Rate  = 0x04;
inline constexpr int Rmp0Range = 0x05;
inline constexpr int Rmp1Rate  = 0x06;
inline constexpr int Rmp1Range = 0x07;
inline constexpr int Pot0      = 0x10;
inline constexpr int Pot1      = 0x11;
inline constexpr int Pot2      = 0x12;
inline constexpr int AdcL      = 0x14;
inline constexpr int AdcR      = 0x15;
inline constexpr int DacL      = 0x16;
inline constexpr int DacR      = 0x17;
inline constexpr int AddrPtr   = 0x18;
inline constexpr int Reg0      = 0x20; // .. Reg31 = 0x3F
inline constexpr int Reg1      = 0x21;
inline constexpr int Reg2      = 0x22;
} // namespace reg

// ---- Instruction encoders (asfv1-identical bit layout) -------------------
// Coefficient arguments are the raw field bits (already fixed-point encoded);
// the assembler/tests convert reals with the s1_14 / s1_9 / s_10 helpers.

constexpr uint32_t opWord(Op op) noexcept { return (uint32_t) op; }

constexpr uint32_t encDelay(Op op, int addr15, int c11) noexcept
{
    return opWord(op) | ((uint32_t) (addr15 & 0x7FFF) << 5) | ((uint32_t) (c11 & 0x7FF) << 21);
}
constexpr uint32_t encRmpa(int c11) noexcept
{
    return opWord(Op::Rmpa) | ((uint32_t) (c11 & 0x7FF) << 21);
}
constexpr uint32_t encRegOp(Op op, int regAddr, int c16) noexcept
{
    return opWord(op) | ((uint32_t) (regAddr & 0x3F) << 5) | ((uint32_t) (c16 & 0xFFFF) << 16);
}
constexpr uint32_t encMulx(int regAddr) noexcept
{
    return opWord(Op::Mulx) | ((uint32_t) (regAddr & 0x3F) << 5);
}
constexpr uint32_t encSofLogExp(Op op, int c16, int d11) noexcept
{
    return opWord(op) | ((uint32_t) (c16 & 0xFFFF) << 16) | ((uint32_t) (d11 & 0x7FF) << 5);
}
constexpr uint32_t encMask(Op op, uint32_t mask24) noexcept
{
    return opWord(op) | ((mask24 & 0xFFFFFF) << 8);
}
constexpr uint32_t encSkp(uint32_t cond5, int n6) noexcept
{
    return opWord(Op::Skp) | ((cond5 & 0x1F) << 27) | ((uint32_t) (n6 & 0x3F) << 21);
}
constexpr uint32_t encWlds(int n1, int f9, int a15) noexcept
{
    return opWord(Op::Wlds) | ((uint32_t) (n1 & 0x1) << 29) | ((uint32_t) (f9 & 0x1FF) << 20)
         | ((uint32_t) (a15 & 0x7FFF) << 5);
}
// n2 is the LFO selector with bit 1 set (RMP0 = 2, RMP1 = 3);
// a2 encodes the range: 0 = 4096, 1 = 2048, 2 = 1024, 3 = 512 (asfv1 mapping).
constexpr uint32_t encWldr(int n2, int f16, int a2) noexcept
{
    return opWord(Op::Wldr) | ((uint32_t) (n2 & 0x3) << 29) | ((uint32_t) (f16 & 0xFFFF) << 13)
         | ((uint32_t) (a2 & 0x3) << 5);
}
constexpr uint32_t encJam(int n2) noexcept // RMP0 = 2, RMP1 = 3
{
    return opWord(Op::Jam) | ((uint32_t) (n2 & 0x3) << 6);
}
constexpr uint32_t encCho(uint32_t type2, int lfo2, uint32_t flags6, int addr16) noexcept
{
    return opWord(Op::Cho) | (type2 << 30) | ((uint32_t) (lfo2 & 0x3) << 21)
         | ((flags6 & 0x3F) << 24) | ((uint32_t) (addr16 & 0xFFFF) << 5);
}

// ---- Fixed-point real -> raw field bits (round-to-nearest, asfv1-style) --
constexpr int fxField(double v, double ref, int mask) noexcept
{
    const double scaled = v * ref;
    const int rounded = (int) (scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
    return rounded & mask;
}
constexpr int s1_14(double v) noexcept { return fxField(v, 16384.0, 0xFFFF); }
constexpr int s1_9(double v) noexcept { return fxField(v, 512.0, 0x7FF); }
constexpr int s_10(double v) noexcept { return fxField(v, 1024.0, 0x7FF); }
constexpr int s_15(double v) noexcept { return fxField(v, 32768.0, 0xFFFF); }

} // namespace s42::fv1
