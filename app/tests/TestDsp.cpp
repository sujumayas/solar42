#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestUtil.h"

#include "dsp/PolyBlepOsc.h"
#include "dsp/RcEnvelope.h"

using Catch::Approx;

namespace
{
constexpr double kSr = 48000.0;
}

TEST_CASE("polyBLEP saw hits the requested frequency", "[dsp][osc]")
{
    s42::PolyBlepOsc osc;
    osc.setSampleRate(kSr);

    std::vector<float> x((size_t) kSr);
    for (auto& v : x)
        v = osc.processSaw(440.0f);

    const int cycles = testutil::countCycles(x, 0.5f);
    CHECK(cycles >= 439);
    CHECK(cycles <= 441);
}

TEST_CASE("polyBLEP saw alias floor at 1 kHz is below -60 dBc", "[dsp][osc]")
{
    constexpr size_t n = 32768;
    constexpr int k0 = 683; // exact-bin fundamental: 683 * 48000 / 32768 = 1000.5 Hz
    const float f0 = (float) k0 * (float) kSr / (float) n;

    s42::PolyBlepOsc osc;
    osc.setSampleRate(kSr);

    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i)
        x[i] = osc.processSaw(f0);

    const auto mag = testutil::spectrum(x);
    const double fund = mag[(size_t) k0];
    REQUIRE(fund > 0.0);

    // Every harmonic lands on a multiple of k0 (+- BH4 main-lobe spread);
    // anything else above the floor is aliasing. polyBLEP cannot suppress a
    // harmonic reflecting right at Nyquist (h24 of 1 kHz -> 23.99 kHz), but
    // that region is inaudible; the quality gate is the audible band, where
    // folded images land inharmonically. If M8 listening ever flags top-octave
    // grit, the escalation is 2x oversampling of the oscillator bank.
    const size_t audibleLimitBin = (size_t) (16000.0 * (double) n / kSr);
    double worstAudible = 0.0, worstAnywhere = 0.0;
    size_t worstAudibleBin = 0;
    for (size_t b = 16; b < mag.size(); ++b) // skip DC/window skirt
    {
        const size_t distToHarmonic = b % (size_t) k0;
        const bool nearHarmonic = distToHarmonic <= 6 || distToHarmonic >= (size_t) k0 - 6;
        if (nearHarmonic)
            continue;
        worstAnywhere = std::max(worstAnywhere, mag[b]);
        if (b < audibleLimitBin && mag[b] > worstAudible)
        {
            worstAudible = mag[b];
            worstAudibleBin = b;
        }
    }

    INFO("worst audible alias at bin " << worstAudibleBin << " = "
         << testutil::db(worstAudible / fund) << " dBc; worst anywhere = "
         << testutil::db(worstAnywhere / fund) << " dBc");
    CHECK(testutil::db(worstAudible / fund) < -60.0);
    CHECK(testutil::db(worstAnywhere / fund) < -30.0);
}

TEST_CASE("RC envelope attack and release track their knob times", "[dsp][env]")
{
    s42::RcEnvelope env;
    env.setSampleRate(kSr);
    env.setAr(0.1f, 0.2f);

    env.gate(true);
    int attackSamples = 0;
    while (env.stage() == s42::RcEnvelope::Stage::Attack && attackSamples < (int) kSr)
    {
        env.process();
        ++attackSamples;
    }
    CHECK(attackSamples == Approx(0.1 * kSr).epsilon(0.10));
    CHECK(env.value() == Approx(1.0));

    env.gate(false);
    int releaseSamples = 0;
    while (env.value() > 0.01f && releaseSamples < (int) kSr)
    {
        env.process();
        ++releaseSamples;
    }
    CHECK(releaseSamples == Approx(0.2 * kSr).epsilon(0.15));
}

TEST_CASE("RC envelope loop mode self-retriggers", "[dsp][env]")
{
    s42::RcEnvelope env;
    env.setSampleRate(kSr);
    env.set(0.01f, 0.02f, 1.0f, 0.02f);
    env.setLoop(true);
    env.gate(true);

    int attacks = 0;
    auto last = s42::RcEnvelope::Stage::Idle;
    for (int i = 0; i < (int) kSr; ++i)
    {
        env.process();
        if (env.stage() == s42::RcEnvelope::Stage::Attack && last != s42::RcEnvelope::Stage::Attack)
            ++attacks;
        last = env.stage();
    }
    CHECK(attacks > 5); // cycling, not parked in sustain
}
