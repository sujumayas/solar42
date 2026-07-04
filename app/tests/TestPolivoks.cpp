#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestUtil.h"

#include "dsp/PolivoksFilter.h"
#include "dsp/Waveshaper.h"
#include "engine/Rack.h"
#include "engine/modules/Mixer.h"

#include <cmath>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double kSr = 48000.0;

// RMS of a filtered sine after the filter settles.
double filteredRms(s42::PolivoksFilter& f, double toneHz, double cutoffHz,
                   float amp, double seconds = 0.5)
{
    const int total = (int) (seconds * kSr);
    const int skip = total / 2;
    double acc = 0.0;
    for (int i = 0; i < total; ++i)
    {
        const float x = amp * (float) std::sin(2.0 * M_PI * toneHz * i / kSr);
        const float y = f.process(x, (float) cutoffHz);
        if (i >= skip)
            acc += (double) y * y;
    }
    return std::sqrt(acc / (total - skip));
}
} // namespace

TEST_CASE("Polivoks LP keeps its bass when resonance rises", "[dsp][filter]")
{
    // The documented trait (42N manual p21): "you will not lose low
    // frequencies when increasing resonance".
    auto rmsAtRes = [](float res)
    {
        s42::PolivoksFilter f;
        f.setSampleRate(kSr);
        f.setRes(res);
        return filteredRms(f, 50.0, 1000.0, 1.0f);
    };

    const double lowRes = rmsAtRes(0.0f);
    const double highRes = rmsAtRes(0.8f);
    INFO("50 Hz through 1 kHz LP: res 0 -> " << lowRes << " Vrms, res 0.8 -> " << highRes);
    CHECK(highRes / lowRes > 0.85); // less than ~1.5 dB of bass lost
    CHECK(highRes / lowRes < 1.6);  // and no runaway either
}

TEST_CASE("Polivoks LP attenuates ~12 dB/oct above cutoff", "[dsp][filter]")
{
    s42::PolivoksFilter f;
    f.setSampleRate(kSr);
    f.setRes(0.2f);

    const double pass = filteredRms(f, 100.0, 800.0, 0.5f);
    f.reset();
    const double twoOctUp = filteredRms(f, 3200.0, 800.0, 0.5f);

    const double att = testutil::db(twoOctUp / pass);
    INFO("2 octaves above cutoff: " << att << " dB");
    CHECK(att < -18.0); // 2-pole ideal is -24 dB; drive/knee nonlinearity allowed slack
    CHECK(att > -34.0);
}

TEST_CASE("Polivoks self-oscillates past the RES threshold, bounded", "[dsp][filter]")
{
    s42::PolivoksFilter f;
    f.setSampleRate(kSr);
    f.setRes(1.0f);

    const double fc = 500.0;
    std::vector<float> tail;
    float peak = 0.0f;
    for (int i = 0; i < (int) kSr * 2; ++i)
    {
        const float y = f.process(0.0f, (float) fc); // no input at all
        peak = std::max(peak, std::abs(y));
        if (i >= (int) kSr)
            tail.push_back(y);
    }

    double acc = 0.0;
    for (float v : tail)
        acc += (double) v * v;
    const double rms = std::sqrt(acc / (double) tail.size());

    INFO("self-osc tail rms = " << rms << " V, peak = " << peak << " V");
    CHECK(rms > 0.05);   // genuinely singing from the noise seed alone
    CHECK(peak < 12.0f); // feedback saturator bounds it below the rails

    // ...and it sings near the cutoff frequency.
    const int cycles = testutil::countCycles(tail, (float) rms);
    INFO("tail cycles/s = " << cycles);
    CHECK(cycles > (int) (fc * 0.7));
    CHECK(cycles < (int) (fc * 1.3));

    // Below the threshold the same filter stays quiet.
    s42::PolivoksFilter g;
    g.setSampleRate(kSr);
    g.setRes(0.5f);
    float quiet = 0.0f;
    for (int i = 0; i < (int) kSr; ++i)
        quiet = std::max(quiet, std::abs(g.process(0.0f, (float) fc)));
    CHECK(quiet < 0.01f);
}

TEST_CASE("Polivoks BP mode picks the band over the extremes", "[dsp][filter]")
{
    s42::PolivoksFilter f;
    f.setSampleRate(kSr);
    f.setRes(0.5f);
    f.setMode(true);

    const double band = filteredRms(f, 1000.0, 1000.0, 0.5f);
    f.reset();
    const double lo = filteredRms(f, 60.0, 1000.0, 0.5f);
    f.reset();
    const double hi = filteredRms(f, 12000.0, 1000.0, 0.5f);

    INFO("BP: band " << band << ", 60 Hz " << lo << ", 12 kHz " << hi);
    CHECK(testutil::db(band / lo) > 10.0);
    CHECK(testutil::db(band / hi) > 10.0);
}

TEST_CASE("double distortion: DIST 0 is bit-clean, full is shaped but bounded", "[dsp][dist]")
{
    for (float v : { -8.0f, -2.0f, 0.0f, 1.5f, 7.0f })
        CHECK(s42::distGain(v, 0.8f, 0.0f) == v); // DIST full-left = clean path only

    // Full dirty: a hot sine comes back flattened (peak limited by the second
    // stage) and asymmetric (even harmonics), but never unbounded.
    float peakPos = 0.0f, peakNeg = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        const float x = 6.0f * (float) std::sin(2.0 * M_PI * 220.0 * i / kSr);
        const float y = s42::distGain(x, 1.0f, 1.0f);
        REQUIRE(std::isfinite(y));
        peakPos = std::max(peakPos, y);
        peakNeg = std::min(peakNeg, y);
    }
    CHECK(peakPos < 12.0f);
    CHECK(-peakNeg < 12.0f);
    CHECK(std::abs(peakPos + peakNeg) > 0.1f); // asymmetric clip == even harmonics
}

TEST_CASE("mixer PAN is the filter routing: hard-left channel reaches only bus L", "[engine][mixer]")
{
    s42::MixerModule mix;
    mix.prepare(kSr);
    s42::MixerModule::Params p;
    for (int c = 0; c < s42::MixerModule::kChannels; ++c)
        p.vol[c] = 0.0f;
    p.vol[s42::MixerModule::ChD1] = 1.0f;
    p.pan[s42::MixerModule::ChD1] = 0.0f;
    mix.setParams(p);

    float in[s42::MixerModule::kChannels] = {};
    in[s42::MixerModule::ChD1] = 1.0f;

    float l = 0.0f, r = 0.0f;
    for (int i = 0; i < 4800; ++i) // let the gain smoothers land
        mix.process(in, l, r);
    CHECK(l == Approx(1.0).margin(0.01));
    CHECK(r == Approx(0.0).margin(0.01));

    // Centre = constant power: both sides at -3 dB, total power preserved.
    p.pan[s42::MixerModule::ChD1] = 0.5f;
    mix.setParams(p);
    for (int i = 0; i < 4800; ++i)
        mix.process(in, l, r);
    CHECK(l == Approx(std::sqrt(0.5)).margin(0.01));
    CHECK(r == Approx(std::sqrt(0.5)).margin(0.01));
}

TEST_CASE("rack: panning DRONE 1 selects which Polivoks it drives", "[engine][mixer]")
{
    auto render = [](float pan)
    {
        s42::Rack rack;
        rack.prepare(kSr, 512);
        s42::Rack::Controls c;
        c.drone[0].hold = true;
        c.drone[0].attackSec = 0.001f;
        for (float& v : c.mixer.vol)
            v = 0.0f;
        c.mixer.vol[s42::MixerModule::ChD1] = 0.7f;
        c.mixer.pan[s42::MixerModule::ChD1] = pan;
        rack.setControls(c);

        std::vector<float> l(48000), r(48000);
        for (int i = 0; i < 48000; i += 512)
            rack.process(l.data() + i, r.data() + i, nullptr, nullptr, 512);

        double el = 0.0, er = 0.0;
        for (size_t i = 24000; i < l.size(); ++i)
        {
            el += (double) l[i] * l[i];
            er += (double) r[i] * r[i];
        }
        return std::pair<double, double>(el, er);
    };

    const auto [hardLeftL, hardLeftR] = render(0.0f);
    CHECK(hardLeftL > 100.0 * hardLeftR); // >20 dB separation into filter L

    const auto [hardRightL, hardRightR] = render(1.0f);
    CHECK(hardRightR > 100.0 * hardRightL);
}
