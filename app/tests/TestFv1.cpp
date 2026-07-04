// FV-1 VM + SpinASM assembler tests.
//
// Assembler goldens: the four Spin AN-0001 programs, asfv1-assembled words
// committed as .hex fixtures (regenerate with tests/fixtures/regen-goldens.sh).
// VM goldens: hand-computed fixed-point results per opcode from the datasheet
// semantics. Program tests: the AN-0001 chorus / pitch-shift idioms run on
// the VM and are measured spectrally — the NA-crossfade octave-up is the
// acceptance test named in the implementation plan.
#include "dsp/PolyphaseResampler.h"
#include "engine/modules/Effector.h"
#include "fv1/CartridgeLibrary.h"
#include "fv1/Fv1Opcodes.h"
#include "fv1/Fv1Vm.h"
#include "fv1/SpinAsm.h"
#include "TestUtil.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using namespace s42::fv1;

namespace
{
std::string readFile(const std::string& path)
{
    std::ifstream in(path);
    REQUIRE(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::array<uint32_t, 128> readGoldenHex(const std::string& path)
{
    std::ifstream in(path);
    REQUIRE(in.good());
    std::array<uint32_t, 128> words {};
    std::string line;
    size_t i = 0;
    while (std::getline(in, line) && i < words.size())
        if (! line.empty())
            words[i++] = (uint32_t) std::stoul(line, nullptr, 16);
    REQUIRE(i == words.size());
    return words;
}

std::array<uint32_t, 128> assembleFixture(const std::string& name, AsmResult* outRes = nullptr)
{
    const auto res = assembleSpinAsm(readFile(std::string(S42_FIXTURES_DIR "/") + name + ".spn"));
    INFO((res.errors.empty() ? std::string("no errors") : res.errors.front()));
    REQUIRE(res.ok());
    if (outRes != nullptr)
        *outRes = res;
    return res.words;
}

// Single-instruction program runner for opcode goldens: set state, run the
// one word, inspect. The trailing pad (SKP 0,0) is harmless.
struct MiniVm
{
    Fv1Vm vm;

    explicit MiniVm(std::initializer_list<uint32_t> ws)
    {
        std::array<uint32_t, 128> prog {};
        size_t i = 0;
        for (uint32_t w : ws)
            prog[i++] = w;
        while (i < prog.size())
            prog[i++] = encSkp(0, 0);
        vm.loadProgram(prog);
    }
};

constexpr int32_t S23(double v) { return (int32_t) std::llround(v * 8388608.0); }
} // namespace

// ---------------------------------------------------------------- assembler

TEST_CASE("SpinASM output is word-identical to asfv1 on the AN-0001 fixtures", "[fv1][asm]")
{
    for (const char* name : { "an0001-1-chorus", "an0001-2-lfowave",
                              "an0001-3-pitchup", "an0001-4-pitchpot" })
    {
        INFO(name);
        const auto ours = assembleFixture(name);
        const auto golden = readGoldenHex(std::string(S42_FIXTURES_DIR "/") + name + ".hex");
        for (size_t i = 0; i < ours.size(); ++i)
        {
            INFO("word " << i);
            REQUIRE(ours[i] == golden[i]);
        }
    }
}

TEST_CASE("SpinASM directives, pseudo-ops and encodings", "[fv1][asm]")
{
    SECTION("EQU works in both orders; MEM allocates n+1 with # and ^ labels")
    {
        const auto res = assembleSpinAsm("equ k 0.5\n"
                                         "gain equ k\n"
                                         "d1 mem 100\n"
                                         "d2 mem 10\n"
                                         "rda d1#,k\n"
                                         "rda d1^,gain\n"
                                         "rda d2,1.0\n");
        REQUIRE(res.ok());
        REQUIRE(res.words[0] == encDelay(Op::Rda, 100, s1_9(0.5)));
        REQUIRE(res.words[1] == encDelay(Op::Rda, 50, s1_9(0.5)));
        REQUIRE(res.words[2] == encDelay(Op::Rda, 101, s1_9(1.0))); // d2 starts at 100+1
    }
    SECTION("pseudo-ops expand as documented")
    {
        const auto res = assembleSpinAsm("clr\nnot\nnop\nabsa\nldax adcl\njmp end\nend: clr\n");
        REQUIRE(res.ok());
        REQUIRE(res.words[0] == encMask(Op::And, 0));
        REQUIRE(res.words[1] == encMask(Op::Xor, 0xFFFFFF));
        REQUIRE(res.words[2] == encSkp(0, 0));
        REQUIRE(res.words[3] == encRegOp(Op::Maxx, 0, 0));
        REQUIRE(res.words[4] == encRegOp(Op::Rdfx, reg::AdcL, 0));
        REQUIRE(res.words[5] == encSkp(0, 0)); // jmp to the next instruction
    }
    SECTION("literals: hex, binary, expressions")
    {
        const auto res = assembleSpinAsm("and $F0F0F0\n"
                                         "or %0000_0001\n"
                                         "rdax reg0,-2.0\n"
                                         "sof 0.25*2,-1.0/2\n");
        REQUIRE(res.ok());
        REQUIRE(res.words[0] == encMask(Op::And, 0xF0F0F0));
        REQUIRE(res.words[1] == encMask(Op::Or, 0x1));
        REQUIRE(res.words[2] == encRegOp(Op::Rdax, 0x20, 0x8000));
        REQUIRE(res.words[3] == encSofLogExp(Op::Sof, s1_14(0.5), s_10(-0.5)));
    }
    SECTION("out-of-range coefficients clamp with a warning")
    {
        const auto res = assembleSpinAsm("rdax reg0,3.0\n");
        REQUIRE(res.ok());
        REQUIRE(! res.warnings.empty());
        REQUIRE(res.words[0] == encRegOp(Op::Rdax, 0x20, 0x7FFF));
    }
    SECTION("errors: undefined labels and skip range")
    {
        REQUIRE(! assembleSpinAsm("rdax nothere,0.5\n").ok());
        REQUIRE(! assembleSpinAsm("skp run,nowhere\n").ok());
    }
    SECTION("CHO flags are masked per LFO type")
    {
        const auto res = assembleSpinAsm("cho rda,sin0,sin|reg|rptr2,10\n"); // RPTR2 invalid on SIN
        REQUIRE(res.ok());
        REQUIRE(! res.warnings.empty());
        REQUIRE(res.words[0] == encCho(cho::TypeRda, 0, cho::Reg, 10));
    }
}

// ---------------------------------------------------------------- VM opcodes

TEST_CASE("FV-1 accumulator opcodes match hand-computed goldens", "[fv1][vm]")
{
    SECTION("SOF scales and offsets")
    {
        MiniVm m({ encSofLogExp(Op::Sof, s1_14(0.5), s_10(0.25)) });
        m.vm.setAcc(S23(0.5));
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == S23(0.5)); // 0.5*0.5 + 0.25
    }
    SECTION("SOF saturates at the S.23 rails")
    {
        MiniVm m({ encSofLogExp(Op::Sof, s1_14(1.999), s_10(0.999)) });
        m.vm.setAcc(0x7FFFFF);
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == 0x7FFFFF);
        m.vm.setAcc(-0x800000);
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == -0x800000);
    }
    SECTION("AND/OR/XOR operate on the 24-bit two's complement image")
    {
        MiniVm m({ encMask(Op::And, 0x800000) });
        m.vm.setAcc(-1); // 0xFFFFFF
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == -0x800000); // sign bit survives, sign-extended

        MiniVm x({ encMask(Op::Xor, 0xFFFFFF) });
        x.vm.setAcc(S23(0.25));
        x.vm.runProgram();
        REQUIRE(x.vm.acc() == ~S23(0.25)); // one's complement in 24 bits
    }
    SECTION("RDAX adds reg*C; RDFX makes the classic one-pole step")
    {
        MiniVm m({ encRegOp(Op::Rdax, reg::Reg0, s1_14(0.25)) });
        m.vm.setReg(reg::Reg0, S23(0.5));
        m.vm.setAcc(S23(0.125));
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == S23(0.25));

        MiniVm f({ encRegOp(Op::Rdfx, reg::Reg1, s1_14(0.5)) });
        f.vm.setReg(reg::Reg1, S23(0.25));
        f.vm.setAcc(S23(0.75));
        f.vm.runProgram();
        REQUIRE(f.vm.acc() == S23(0.5)); // (0.75-0.25)*0.5 + 0.25
    }
    SECTION("WRAX writes then scales; WRHX/WRLX blend with PACC")
    {
        // ldax-style load puts input in ACC; second instruction sees PACC.
        MiniVm h({ encRegOp(Op::Rdfx, reg::Reg0, s1_14(0.5)),
                   encRegOp(Op::Wrhx, reg::Reg1, s1_14(-0.5)) });
        h.vm.setAcc(S23(0.5));       // "input" x
        h.vm.setReg(reg::Reg0, 0);   // filter state 0 -> rdfx: 0.5*0.5 = 0.25
        h.vm.runProgram();
        // wrhx: reg1 = 0.25; acc = 0.25*-0.5 + PACC(x=0.5) = 0.375
        REQUIRE(h.vm.regValue(reg::Reg1) == S23(0.25));
        REQUIRE(h.vm.acc() == S23(0.375));

        MiniVm l({ encRegOp(Op::Rdfx, reg::Reg0, s1_14(0.5)),
                   encRegOp(Op::Wrlx, reg::Reg1, s1_14(-1.0)) });
        l.vm.setAcc(S23(0.5));
        l.vm.runProgram();
        // wrlx: acc = (PACC - acc)*C + PACC = (0.5-0.25)*-1 + 0.5 = 0.25
        REQUIRE(l.vm.acc() == S23(0.25));
    }
    SECTION("MAXX takes the larger magnitude; ABSA form rectifies")
    {
        MiniVm m({ encRegOp(Op::Maxx, reg::Reg0, s1_14(1.0)) });
        m.vm.setReg(reg::Reg0, S23(-0.5));
        m.vm.setAcc(S23(0.25));
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == S23(0.5));

        MiniVm a({ encRegOp(Op::Maxx, 0, 0) }); // ABSA
        a.vm.setAcc(S23(-0.75));
        a.vm.runProgram();
        REQUIRE(a.vm.acc() == S23(0.75));
    }
    SECTION("MULX uses only the top 15 bits of the register")
    {
        MiniVm m({ encMulx(reg::Reg0) });
        m.vm.setAcc(S23(0.5));
        m.vm.setReg(reg::Reg0, 511); // below bit 9 -> coefficient is exactly 0
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == 0);

        MiniVm p({ encMulx(reg::Reg0) });
        p.vm.setAcc(S23(0.5));
        p.vm.setReg(reg::Reg0, S23(0.5));
        p.vm.runProgram();
        REQUIRE(p.vm.acc() == S23(0.25));
    }
    SECTION("LOG/EXP round-trip on exact powers of two")
    {
        MiniVm lg({ encSofLogExp(Op::Log, s1_14(1.0), s_10(0.0)) });
        lg.vm.setAcc(S23(0.25));
        lg.vm.runProgram();
        REQUIRE(lg.vm.acc() == S23(-0.125)); // log2(0.25)/16

        MiniVm ex({ encSofLogExp(Op::Exp, s1_14(1.0), s_10(0.0)) });
        ex.vm.setAcc(S23(-0.125));
        ex.vm.runProgram();
        REQUIRE(ex.vm.acc() == S23(0.25));

        // positive input saturates EXP
        ex.vm.setAcc(S23(0.1));
        ex.vm.runProgram();
        REQUIRE(ex.vm.acc() == 0x7FFFFF);
    }
}

TEST_CASE("FV-1 delay opcodes and the down-counting pointer", "[fv1][vm]")
{
    SECTION("WRA then RDA next sample reads one address up")
    {
        // sample 1 writes ACC to addr 0; pointer decrements; sample 2 reads addr 1
        MiniVm m({ encRegOp(Op::Rdfx, reg::AdcL, 0), // ldax adcl
                   encDelay(Op::Wra, 0, s1_9(0.0)),
                   encDelay(Op::Rda, 1, s1_9(1.0)),
                   encRegOp(Op::Wrax, reg::DacL, 0) });
        float l = 0, r = 0;
        m.vm.processSample(0.5f, 0.0f, l, r);
        REQUIRE(l == 0.0f); // nothing there yet
        m.vm.processSample(0.0f, 0.0f, l, r);
        REQUIRE_THAT((double) l, Catch::Matchers::WithinAbs(0.5, 1e-6));
    }
    SECTION("WRAP adds LR for the all-pass idiom")
    {
        MiniVm m({ encRegOp(Op::Rdfx, reg::AdcL, 0), // ldax adcl
                   encDelay(Op::Wra, 5, s1_9(0.0)),
                   encDelay(Op::Rda, 5, s1_9(0.5)),
                   encDelay(Op::Wrap, 4, s1_9(-0.5)),
                   encRegOp(Op::Wrax, reg::DacL, 0) });
        float l = 0, r = 0;
        m.vm.processSample(0.5f, 0.0f, l, r);
        // rda:  acc = 0 + 0.5*0.5 = 0.25 (LR = 0.5)
        // wrap: mem[4] = 0.25; acc = 0.25*-0.5 + LR = 0.375
        REQUIRE_THAT((double) l, Catch::Matchers::WithinAbs(0.375, 1e-6));
    }
    SECTION("RMPA reads via ADDR_PTR")
    {
        // OR builds the pointer directly: ADDR_PTR wants address * 256.
        MiniVm m({ encRegOp(Op::Rdfx, reg::AdcL, 0),
                   encDelay(Op::Wra, 40, s1_9(0.0)),
                   encMask(Op::Or, 40u << 8),
                   encRegOp(Op::Wrax, reg::AddrPtr, 0),
                   encRmpa(s1_9(1.0)),
                   encRegOp(Op::Wrax, reg::DacL, 0) });
        float l = 0, r = 0;
        m.vm.processSample(0.25f, 0.0f, l, r);
        REQUIRE_THAT((double) l, Catch::Matchers::WithinAbs(0.25, 1e-5));
    }
}

TEST_CASE("FV-1 SKP conditions", "[fv1][vm]")
{
    auto run = [](uint32_t cond, int32_t accVal, bool expectSkip) {
        // skp cond,1 over a CLR; if skipped ACC survives
        MiniVm m({ encSkp(cond, 1), encMask(Op::And, 0) });
        m.vm.setAcc(accVal);
        m.vm.runProgram();
        if (expectSkip)
            REQUIRE(m.vm.acc() == accVal);
        else
            REQUIRE(m.vm.acc() == 0);
    };
    run(skp::Neg, S23(-0.5), true);
    run(skp::Neg, S23(0.5), false);
    run(skp::Gez, S23(0.5), true);
    run(skp::Gez, S23(-0.5), false);
    run(skp::Zro, 0, true);
    run(skp::Zro, 1, false);
    run(0, S23(0.5), true); // empty mask is vacuously true (JMP pseudo-op)

    SECTION("RUN is false only on the very first execution")
    {
        MiniVm m({ encSkp(skp::Run, 1), encMask(Op::And, 0) });
        m.vm.setAcc(S23(0.5));
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == 0); // first pass: no skip, CLR runs
        m.vm.stepSampleCounter();
        m.vm.setAcc(S23(0.5));
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == S23(0.5)); // now it skips
    }
    SECTION("ZRC detects a sign change against PACC")
    {
        // sof -1,0 flips the sign -> PACC positive, ACC negative at the skip
        MiniVm m({ encSofLogExp(Op::Sof, s1_14(-1.0), s_10(0.0)),
                   encSkp(skp::Zrc, 1), encMask(Op::And, 0) });
        m.vm.setAcc(S23(0.5));
        m.vm.runProgram();
        REQUIRE(m.vm.acc() == S23(-0.5));
    }
}

// ------------------------------------------------------------- VM + programs

TEST_CASE("AN-0001 chorus program runs and passes audio", "[fv1][vm][program]")
{
    Fv1Vm vm;
    vm.loadProgram(assembleFixture("an0001-1-chorus"));

    // 440 Hz-equivalent sine at the chip rate.
    const double w = 2.0 * M_PI * 440.0 / Fv1Vm::kClockHz;
    std::vector<float> out;
    out.reserve(16384);
    float l = 0, r = 0;
    for (int i = 0; i < 8192 + 16384; ++i)
    {
        vm.processSample((float) (0.5 * std::sin(w * i)), 0.0f, l, r);
        if (i >= 8192)
            out.push_back(l);
    }
    // The modulated tap keeps the level; the AN-0001 depth is deliberately
    // extreme (+-4096 samples at 0.2 Hz -> up to ~16 % Doppler), so only pin
    // the pitch to a wide window around the source.
    double rms = 0;
    for (float v : out)
        rms += (double) v * v;
    rms = std::sqrt(rms / (double) out.size());
    REQUIRE_THAT(rms, Catch::Matchers::WithinAbs(0.5 / std::sqrt(2.0), 0.05));

    const auto mag = testutil::spectrum(out);
    const size_t peak = (size_t) (std::max_element(mag.begin(), mag.end()) - mag.begin());
    const double peakHz = (double) peak * Fv1Vm::kClockHz / (double) (2 * mag.size());
    REQUIRE(peakHz > 340.0);
    REQUIRE(peakHz < 470.0);
}

TEST_CASE("AN-0001 ramp pitch shifter produces a clean octave up", "[fv1][vm][program]")
{
    Fv1Vm vm;
    vm.loadProgram(assembleFixture("an0001-3-pitchup"));

    const double fIn = 500.0;
    const double w = 2.0 * M_PI * fIn / Fv1Vm::kClockHz;
    std::vector<float> out;
    out.reserve(32768);
    float l = 0, r = 0;
    for (int i = 0; i < 8192 + 32768; ++i)
    {
        vm.processSample((float) (0.5 * std::sin(w * i)), 0.0f, l, r);
        if (i >= 8192)
            out.push_back(l);
    }

    const auto mag = testutil::spectrum(out);
    const auto hzPerBin = Fv1Vm::kClockHz / (double) (2 * mag.size());
    const size_t peak = (size_t) (std::max_element(mag.begin(), mag.end()) - mag.begin());
    const double peakHz = (double) peak * hzPerBin;
    INFO("peak at " << peakHz << " Hz");
    REQUIRE_THAT(peakHz, Catch::Matchers::WithinAbs(2.0 * fIn, 15.0));

    // Residual input pitch must sit well below the shifted line.
    const size_t inBin = (size_t) std::llround(fIn / hzPerBin);
    double inMag = 0;
    for (size_t b = inBin - 2; b <= inBin + 2; ++b)
        inMag = std::max(inMag, mag[b]);
    REQUIRE(testutil::db(inMag / mag[peak]) < -20.0);
}

// --------------------------------------------------------------- cartridges

TEST_CASE("all 12 embedded cartridge programs assemble clean", "[fv1][cartridge]")
{
    using CL = s42::fv1::CartridgeLibrary;
    for (int c = 0; c < CL::kCartridges; ++c)
        for (int p = 0; p < CL::kProgramsPerCartridge; ++p)
        {
            const auto& e = CL::entry(c, p);
            INFO(e.cartridge << " / " << e.program);
            const auto res = assembleSpinAsm(e.source);
            for (const auto& err : res.errors)
                INFO(err);
            REQUIRE(res.ok());
            REQUIRE(res.warnings.empty());
            REQUIRE(res.instructionCount <= 128);
            REQUIRE(CL::words(c, p)[0] != 0);
        }
}

namespace
{
double bandEnergy(const std::vector<double>& mag, double hzPerBin, double lo, double hi)
{
    double e = 0;
    for (size_t b = (size_t) (lo / hzPerBin); b <= (size_t) (hi / hzPerBin) && b < mag.size(); ++b)
        e += mag[b] * mag[b];
    return e;
}

// Feed a sine at the chip rate, return the last `keep` output samples.
std::vector<float> runVmOnSine(Fv1Vm& vm, double freq, float amp, int total, int keep)
{
    std::vector<float> out;
    out.reserve((size_t) keep);
    float l = 0, r = 0;
    for (int i = 0; i < total; ++i)
    {
        vm.processSample((float) (amp * std::sin(2.0 * M_PI * freq * i / Fv1Vm::kClockHz)),
                         0.0f, l, r);
        if (i >= total - keep)
            out.push_back(l);
    }
    return out;
}
} // namespace

TEST_CASE("CATHEDRAL shimmer blooms an octave above the source", "[fv1][cartridge]")
{
    using CL = s42::fv1::CartridgeLibrary;
    const double fIn = 400.0;

    auto octaveBandRatio = [&](float potX) {
        Fv1Vm vm;
        vm.loadProgram(CL::words(0, 0)); // CATHEDRAL / SHIMMER
        vm.setPot(0, potX);  // oct-up level
        vm.setPot(1, 0.0f);  // oct-down off
        vm.setPot(2, 0.8f);  // long decay
        const auto out = runVmOnSine(vm, fIn, 0.25f, 3 * 32768, 32768);
        const auto mag = testutil::spectrum(out);
        const double hzPerBin = Fv1Vm::kClockHz / (double) (2 * mag.size());
        const double oct = bandEnergy(mag, hzPerBin, 720.0, 880.0);
        const double fund = bandEnergy(mag, hzPerBin, 360.0, 440.0);
        return oct / fund;
    };

    const double withShimmer = octaveBandRatio(1.0f);
    const double withoutShimmer = octaveBandRatio(0.0f);
    INFO("octave/fundamental with X=1: " << withShimmer << ", X=0: " << withoutShimmer);
    REQUIRE(withShimmer > 4.0 * withoutShimmer);
    REQUIRE(withShimmer > 0.05);
}

TEST_CASE("OCHRE reverse one-shot triggers, mutes, and re-arms", "[fv1][cartridge]")
{
    using CL = s42::fv1::CartridgeLibrary;
    Fv1Vm vm;
    vm.loadProgram(CL::words(3, 0)); // OCHRE / REVERSE ONE-SHOT LONG
    vm.setPot(0, 0.5f); // sweep speed
    vm.setPot(1, 0.0f); // no feedback
    vm.setPot(2, 0.3f); // trigger threshold

    const double f = 300.0;
    auto rmsOver = [&](int samples, bool tone) {
        double acc = 0;
        float l = 0, r = 0;
        for (int i = 0; i < samples; ++i)
        {
            const float x = tone ? (float) (0.5 * std::sin(2.0 * M_PI * f * i / Fv1Vm::kClockHz))
                                 : 0.0f;
            vm.processSample(x, 0.0f, l, r);
            acc += (double) l * l;
        }
        return std::sqrt(acc / samples);
    };

    const double armedSilence = rmsOver(6554, false);  // 0.2 s: parked + armed
    const double firstShot = rmsOver(16384, true);     // burst -> fires, plays reverse
    rmsOver(26000, false);                             // sweep finishes, env decays, re-arms
    const double mutedSilence = rmsOver(6554, false);  // still silent
    const double secondShot = rmsOver(16384, true);    // re-triggered

    INFO("armed " << armedSilence << " first " << firstShot << " muted " << mutedSilence
                  << " second " << secondShot);
    REQUIRE(armedSilence < 1e-4);
    REQUIRE(firstShot > 0.05);
    REQUIRE(mutedSilence < 1e-4);
    REQUIRE(secondShot > 0.05);
}

TEST_CASE("EffectorModule: dry at blend 0, unity-ish wet through the full chain", "[fv1][effector]")
{
    const double hostRate = 48000.0;
    const double freq = 1000.0;
    s42::Tolerances tol;

    auto run = [&](s42::EffectorModule::Params p) {
        s42::EffectorModule fx;
        fx.prepare(hostRate, tol);
        fx.setParams(p);
        std::vector<float> out;
        out.reserve(48000);
        for (int i = 0; i < 48000; ++i)
        {
            float l = (float) (2.0 * std::sin(2.0 * M_PI * freq * i / hostRate)); // 2 V sine
            float r = l;
            fx.process(l, r, 0.0f, 0.0f, 0.0f);
            if (i >= 4800)
                out.push_back(l);
        }
        double rms = 0;
        for (float v : out)
            rms += (double) v * v;
        return std::sqrt(rms / (double) out.size());
    };

    s42::EffectorModule::Params dry;
    dry.blend = 0.0f;
    const double dryRms = run(dry);
    REQUIRE_THAT(dryRms, Catch::Matchers::WithinAbs(2.0 / std::sqrt(2.0), 0.01));

    // VIBROTREM tremolo with depth 0 and no plate = a wire through the chip:
    // resamplers + fixed-point VM + volts scaling should come back unity-ish.
    // Faithful slot semantics: inserting a cartridge loads nothing until a
    // 1-2-3 toggle flips, so the test flips them like a hardware user would.
    const double wetRms = [&] {
        s42::EffectorModule fx;
        fx.prepare(hostRate, tol);
        s42::EffectorModule::Params wet;
        wet.cartridge = 2;
        wet.progL = wet.progR = 1; // flip away...
        fx.setParams(wet);
        wet.progL = wet.progR = 0; // ...and back to program 1 (TREMOLO)
        wet.x = 0.0f;
        wet.z = 0.0f;
        wet.blend = 1.0f;
        fx.setParams(wet);
        std::vector<float> out;
        out.reserve(48000);
        for (int i = 0; i < 48000; ++i)
        {
            float l = (float) (2.0 * std::sin(2.0 * M_PI * freq * i / hostRate));
            float r = l;
            fx.process(l, r, 0.0f, 0.0f, 0.0f);
            if (i >= 4800)
                out.push_back(l);
        }
        double rms = 0;
        for (float v : out)
            rms += (double) v * v;
        return std::sqrt(rms / (double) out.size());
    }();
    INFO("wet RMS " << wetRms);
    REQUIRE_THAT(wetRms, Catch::Matchers::WithinAbs(2.0 / std::sqrt(2.0), 0.15));
}

// ---------------------------------------------------------------- resampler

namespace
{
// SNR against the best least-squares sine fit at the known frequency —
// window-free, so the measurement floor is far below the 90 dB requirement.
double sineFitSnrDb(const std::vector<float>& y, double freq, double rate)
{
    const size_t skip = 512; // edge transients
    double ss = 0, sc = 0, cc = 0, ys = 0, yc = 0;
    for (size_t i = skip; i + skip < y.size(); ++i)
    {
        const double p = 2.0 * M_PI * freq * (double) i / rate;
        const double s = std::sin(p), c = std::cos(p);
        ss += s * s; sc += s * c; cc += c * c;
        ys += (double) y[i] * s; yc += (double) y[i] * c;
    }
    const double det = ss * cc - sc * sc;
    const double a = (ys * cc - yc * sc) / det;
    const double b = (yc * ss - ys * sc) / det;
    double sig = 0, err = 0;
    for (size_t i = skip; i + skip < y.size(); ++i)
    {
        const double p = 2.0 * M_PI * freq * (double) i / rate;
        const double fit = a * std::sin(p) + b * std::cos(p);
        sig += fit * fit;
        err += ((double) y[i] - fit) * ((double) y[i] - fit);
    }
    return 10.0 * std::log10(sig / err);
}
} // namespace

TEST_CASE("PolyphaseResampler round trip exceeds 90 dB SNR", "[fv1][resampler]")
{
    for (const double hostRate : { 48000.0, 44100.0, 96000.0 })
    {
        INFO("host rate " << hostRate);
        s42::PolyphaseResampler down, up;
        down.prepare(hostRate, s42::fv1::Fv1Vm::kClockHz);
        up.prepare(s42::fv1::Fv1Vm::kClockHz, hostRate);

        const double freq = 2000.0;
        std::vector<float> out;
        out.reserve(1 << 16);
        float mid[8], back[8];
        for (int i = 0; i < (1 << 16); ++i)
        {
            const auto x = (float) (0.5 * std::sin(2.0 * M_PI * freq * i / hostRate));
            const int nMid = down.push(x, mid, 8);
            for (int j = 0; j < nMid; ++j)
            {
                const int nBack = up.push(mid[j], back, 8);
                for (int k = 0; k < nBack; ++k)
                    out.push_back(back[k]);
            }
        }
        REQUIRE(out.size() > (1u << 15));
        const double snr = sineFitSnrDb(out, freq, hostRate);
        INFO("SNR " << snr << " dB");
        REQUIRE(snr > 90.0);
    }
}

TEST_CASE("PolyphaseResampler holds the clock-skewed ratio", "[fv1][resampler]")
{
    // +0.05 % chip clock: over one second the skewed VM consumes ~16 more
    // samples than the nominal one — the "two slightly different chips" trick.
    s42::PolyphaseResampler nominal, skewed;
    nominal.prepare(48000.0, 32768.0);
    skewed.prepare(48000.0, 32768.0 * 1.0005);

    float buf[8];
    int64_t nNominal = 0, nSkewed = 0;
    for (int i = 0; i < 48000; ++i)
    {
        nNominal += nominal.push(0.0f, buf, 8);
        nSkewed += skewed.push(0.0f, buf, 8);
    }
    REQUIRE(nSkewed - nNominal >= 12);
    REQUIRE(nSkewed - nNominal <= 20);
}

TEST_CASE("AN-0001 LFO wave program: POTs steer the SIN LFO rate live", "[fv1][vm][program]")
{
    Fv1Vm vm;
    vm.loadProgram(assembleFixture("an0001-2-lfowave"));
    vm.setPot(0, 1.0f);  // full amplitude
    vm.setPot(1, 0.5f);  // mid rate

    std::vector<float> out;
    out.reserve(32768);
    float l = 0, r = 0;
    for (int i = 0; i < 1024 + 32768; ++i)
    {
        vm.processSample(0.0f, 0.0f, l, r);
        if (i >= 1024)
            out.push_back(l);
    }
    // Kf ~= (0.499*0.7338 + 0.2446)*512 ~= 312 -> ~12.4 Hz; one second captured.
    const int cycles = testutil::countCycles(out, 0.3f);
    REQUIRE(cycles >= 11);
    REQUIRE(cycles <= 14);

    // Faster pot position must raise the rate.
    vm.setPot(1, 1.0f);
    out.clear();
    for (int i = 0; i < 1024 + 32768; ++i)
    {
        vm.processSample(0.0f, 0.0f, l, r);
        if (i >= 1024)
            out.push_back(l);
    }
    REQUIRE(testutil::countCycles(out, 0.3f) > cycles + 4);
}
