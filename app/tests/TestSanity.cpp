#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "dsp/Volts.h"
#include "dsp/TuningConstants.h"
#include "dsp/Tolerances.h"
#include "engine/Rack.h"

#include <cmath>
#include <vector>

using Catch::Approx;

TEST_CASE("volt/pitch conversions", "[dsp]")
{
    CHECK(s42::voltToRatio(1.0f) == Approx(2.0));
    CHECK(s42::voltToRatio(0.0f) == Approx(1.0));
    CHECK(s42::voctToHz(0.0f) == Approx(16.3515978313));
    CHECK(s42::voctToHz(4.0f + 9.0f / 12.0f) == Approx(440.0).margin(0.01)); // A4 at 4.75 V
}

TEST_CASE("rail clamp is bounded, odd, and transparent at small signals", "[dsp]")
{
    CHECK(std::abs(s42::railClamp(1000.0f)) <= s42::kRailVolts);
    CHECK(std::abs(s42::railClamp(-1000.0f)) <= s42::kRailVolts);
    CHECK(s42::railClamp(-5.0f) == Approx(-s42::railClamp(5.0f)));
    CHECK(s42::railClamp(0.5f) == Approx(0.5).margin(0.002)); // near-linear in-range
}

TEST_CASE("tolerances are deterministic per unit serial and component", "[dsp]")
{
    s42::Tolerances a { 42 }, b { 42 }, c { 43 };
    constexpr auto id = s42::Tolerances::id("filtL.freq");

    CHECK(a.bipolar(id) == b.bipolar(id));
    CHECK(a.bipolar(id) != c.bipolar(id));
    CHECK(a.bipolar(id) >= -1.0f);
    CHECK(a.bipolar(id) <= 1.0f);
    CHECK(a.spread(id, 0.025f) == Approx(1.0).margin(0.025));
    CHECK(a.bipolar(s42::Tolerances::id("filtR.freq")) != a.bipolar(id));
}

TEST_CASE("M0 rack renders silence without touching out-of-range memory", "[engine]")
{
    s42::Rack rack;
    rack.prepare(48000.0, 512);

    std::vector<float> l(512, 1.0f), r(512, 1.0f);
    rack.process(l.data(), r.data(), nullptr, nullptr, 512);

    // Since M3 the Polivoks filters carry a deliberate ~-180 dB thermal-noise
    // seed (it starts their self-oscillation like op-amp noise does on the
    // hardware), so "silence" means below any audible floor, not bit-zero.
    for (int i = 0; i < 512; ++i)
    {
        REQUIRE(std::abs(l[(size_t) i]) < 1.0e-6f);
        REQUIRE(std::abs(r[(size_t) i]) < 1.0e-6f);
    }
}
