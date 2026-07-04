#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestUtil.h"

#include "dsp/Tolerances.h"
#include "engine/modules/ClassicDroneVoice.h"

#include <cmath>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double kSr = 48000.0;

s42::ClassicDroneVoice::Params quietStart()
{
    s42::ClassicDroneVoice::Params p;
    p.attackSec = 0.01f;
    p.releaseSec = 0.05f;
    return p;
}

float rms(const std::vector<float>& x)
{
    double acc = 0.0;
    for (float v : x)
        acc += (double) v * v;
    return (float) std::sqrt(acc / (double) x.size());
}
} // namespace

TEST_CASE("drone voice sounds when gated and dies when released", "[engine][drone]")
{
    s42::Tolerances tol { 7 };
    s42::ClassicDroneVoice voice;
    voice.prepare(kSr, tol, 1);
    voice.setParams(quietStart());

    std::vector<float> on(24000), off(24000);
    for (auto& v : on)
        v = voice.process(true, 0.0f);
    CHECK(rms(on) > 0.5f); // 5 gens at 2 V peak each: healthy volts

    for (auto& v : off)
        v = voice.process(false, 0.0f);
    float tailRms = rms({ off.end() - 4800, off.end() });
    CHECK(tailRms < 0.01f);
}

TEST_CASE("HOLD latches the gate", "[engine][drone]")
{
    s42::Tolerances tol { 7 };
    s42::ClassicDroneVoice voice;
    voice.prepare(kSr, tol, 1);
    auto p = quietStart();
    p.hold = true;
    voice.setParams(p);

    std::vector<float> out(24000);
    for (auto& v : out)
        v = voice.process(false, 0.0f); // no gate — hold alone sustains
    CHECK(rms({ out.end() - 4800, out.end() }) > 0.5f);
}

TEST_CASE("muting all generators silences the voice", "[engine][drone]")
{
    s42::Tolerances tol { 7 };
    s42::ClassicDroneVoice voice;
    voice.prepare(kSr, tol, 1);
    auto p = quietStart();
    for (auto& m : p.mute)
        m = true;
    voice.setParams(p);

    std::vector<float> out(9600);
    for (auto& v : out)
        v = voice.process(true, 0.0f);
    CHECK(rms({ out.end() - 4800, out.end() }) < 0.005f);
}

TEST_CASE("generator 5 tracks its factory range", "[engine][drone]")
{
    s42::Tolerances tol { 7 };
    s42::ClassicDroneVoice voice;
    voice.prepare(kSr, tol, 1);
    auto p = quietStart();
    for (int g = 0; g < 4; ++g)
        p.mute[g] = true;   // solo gen 5
    p.tune[4] = 0.0f;       // bottom of range: G5 = 784 Hz
    p.attackSec = 0.001f;
    voice.setParams(p);

    for (int i = 0; i < 4800; ++i)
        voice.process(true, 0.0f); // settle smoothers/envelope

    std::vector<float> x((size_t) kSr);
    for (auto& v : x)
        v = voice.process(true, 0.0f);

    // Amplitude ~2 V (kGenLevelVolts); hysteresis at +-1 V.
    const int cycles = testutil::countCycles(x, 1.0f);
    CHECK((float) cycles == Approx(784.0f).epsilon(0.02)); // +-2 % (unit tolerance is +-8 cents)
}

TEST_CASE("VOLT dirty zone stays bounded and changes the signal", "[engine][drone]")
{
    s42::Tolerances tol { 7 };

    auto render = [&](float volt)
    {
        s42::ClassicDroneVoice voice;
        voice.prepare(kSr, tol, 1);
        auto p = quietStart();
        p.volt = volt;
        p.attackSec = 0.001f;
        voice.setParams(p);
        std::vector<float> out(24000);
        for (auto& v : out)
            v = voice.process(true, 0.0f);
        return out;
    };

    const auto clean = render(0.3f);
    const auto dirty = render(0.95f);

    for (float v : dirty)
        REQUIRE(std::abs(v) < 12.0f);

    // Same knobs, different zone -> audibly different signal.
    double diff = 0.0;
    for (size_t i = 12000; i < clean.size(); ++i)
        diff += std::abs((double) clean[i] - (double) dirty[i]);
    CHECK(diff / (double) clean.size() > 0.01);
}
