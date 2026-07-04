#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestUtil.h"

#include "dsp/PhotoSensor.h"
#include "dsp/Tolerances.h"
#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"
#include "engine/modules/PapaSrapaVoice.h"
#include "engine/modules/VcoVoice.h"

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double kSr = 48000.0;

// Render a Papa Srapa voice (HOLD latched, no S&H clock) and drop the
// parameter-smoother transient.
std::vector<float> renderSrapa(const s42::PapaSrapaVoice::Params& p, double seconds = 1.5)
{
    s42::Tolerances tol;
    s42::PapaSrapaVoice v;
    v.setParams(p);
    v.prepare(kSr, tol, 3);

    const int total = (int) (seconds * kSr);
    const int skip = (int) (0.4 * kSr);
    std::vector<float> x;
    x.reserve((size_t) (total - skip));
    for (int i = 0; i < total; ++i)
    {
        const float y = v.process(false, false, 0.0f, 0.0f);
        if (i >= skip)
            x.push_back(y);
    }
    return x;
}

s42::PapaSrapaVoice::Params srapaBase()
{
    s42::PapaSrapaVoice::Params p;
    p.hold = true;
    p.attackSec = 0.001f;
    p.pitch01 = 0.5f;
    p.rate01 = 0.642f; // ~2 Hz on the x1 range
    p.fmAmt = 1.0f;
    return p;
}

// Cycle rate per 100 ms window — shows FM pitch alternation.
std::vector<int> windowedCycles(const std::vector<float>& x)
{
    const size_t win = (size_t) (0.1 * kSr);
    std::vector<int> rates;
    for (size_t s = 0; s + win <= x.size(); s += win)
        rates.push_back(testutil::countCycles({ x.begin() + (long) s,
                                                x.begin() + (long) (s + win) }, 1.0f));
    return rates;
}

// RMS per 50 ms window — shows AM chopping.
std::vector<double> windowedRms(const std::vector<float>& x)
{
    const size_t win = (size_t) (0.05 * kSr);
    std::vector<double> out;
    for (size_t s = 0; s + win <= x.size(); s += win)
    {
        double acc = 0.0;
        for (size_t i = s; i < s + win; ++i)
            acc += (double) x[i] * x[i];
        out.push_back(std::sqrt(acc / (double) win));
    }
    return out;
}
} // namespace

// ---------------------------------------------------------------- Papa Srapa

TEST_CASE("srapa mode 1: plain drone — steady pitch, never-50% duty", "[engine][srapa]")
{
    const auto x = renderSrapa(srapaBase());

    // PITCH 0.5, low range: ~C0 + 2.67 oct ≈ 104 Hz.
    const double expectHz = 16.3516 * std::exp2(0.5 * s42::tuning::kSrapaPitchSpanOct);
    const int cycles = testutil::countCycles(x, 1.0f);
    const double seconds = (double) x.size() / kSr;
    INFO("expected ~" << expectHz << " Hz, measured " << cycles / seconds);
    CHECK(cycles / seconds == Approx(expectHz).epsilon(0.10));

    // Relaxation oscillator: charge/discharge asymmetry means non-50 % duty.
    size_t high = 0;
    for (float v : x)
        if (v > 0.0f)
            ++high;
    const double duty = (double) high / (double) x.size();
    INFO("duty = " << duty);
    CHECK(duty > 0.51);
    CHECK(duty < 0.72);
}

TEST_CASE("srapa mode 2: FM — pitch alternates at the divided RATE", "[engine][srapa]")
{
    auto p = srapaBase();
    p.fmOn = true;
    const auto rates = windowedCycles(renderSrapa(p));

    int lo = 1 << 30, hi = 0;
    for (int r : rates)
    {
        lo = std::min(lo, r);
        hi = std::max(hi, r);
    }
    INFO("windowed cycle counts " << lo << " .. " << hi);
    CHECK(hi >= 2 * lo); // full FM AMOUNT jumps the pitch by octaves
}

TEST_CASE("srapa mode 3: AM — tone chopped at RATE", "[engine][srapa]")
{
    auto p = srapaBase();
    p.amOn = true;
    const auto rms = windowedRms(renderSrapa(p));

    int quiet = 0, loud = 0;
    for (double v : rms)
    {
        if (v < 0.4)
            ++quiet;
        if (v > 1.5)
            ++loud;
    }
    INFO("quiet windows " << quiet << ", loud windows " << loud << " of " << rms.size());
    CHECK(quiet >= 2);
    CHECK(loud >= 2);
}

TEST_CASE("srapa mode 4: FM+AM — chopped and wobbling at once", "[engine][srapa]")
{
    auto p = srapaBase();
    p.fmOn = p.amOn = true;
    const auto x = renderSrapa(p);

    for (float v : x)
        REQUIRE(std::isfinite(v));

    const auto rms = windowedRms(x);
    int quiet = 0;
    for (double v : rms)
        if (v < 0.4)
            ++quiet;
    CHECK(quiet >= 2); // still intermittent

    const auto rates = windowedCycles(x);
    int lo = 1 << 30, hi = 0;
    for (int r : rates)
    {
        lo = std::min(lo, r);
        hi = std::max(hi, r);
    }
    CHECK(hi >= 2 * lo); // still wobbling
}

TEST_CASE("srapa mode 5: NOISE knob replaces the oscillator with white noise", "[engine][srapa]")
{
    const auto plain = renderSrapa(srapaBase());
    auto p = srapaBase();
    p.noise01 = 1.0f;
    const auto noisy = renderSrapa(p);

    const int plainCycles = testutil::countCycles(plain, 1.0f);
    const int noisyCycles = testutil::countCycles(noisy, 1.0f);
    INFO("plain " << plainCycles << " cycles, noise " << noisyCycles);
    CHECK(noisyCycles > 20 * plainCycles); // aperiodic wideband, not a tone
}

TEST_CASE("srapa cv out is a unipolar 0..12 V square", "[engine][srapa]")
{
    s42::Tolerances tol;
    s42::PapaSrapaVoice v;
    auto p = srapaBase();
    v.setParams(p);
    v.prepare(kSr, tol, 6);

    float mn = 1.0e9f, mx = -1.0e9f;
    for (int i = 0; i < (int) kSr; ++i)
    {
        v.process(false, false, 0.0f, 0.0f);
        mn = std::min(mn, v.cvOutVolts());
        mx = std::max(mx, v.cvOutVolts());
    }
    CHECK(mn == 0.0f);
    CHECK(mx == 12.0f);
}

TEST_CASE("srapa S&H samples its own noise only on clock rising edges", "[engine][srapa]")
{
    s42::Tolerances tol;
    s42::PapaSrapaVoice v;
    v.setParams(srapaBase());
    v.prepare(kSr, tol, 3);

    std::vector<float> held;
    std::vector<size_t> edges;
    const size_t half = 200;
    for (size_t i = 0; i < 4000; ++i)
    {
        const bool clockHigh = (i / half) % 2 == 1;
        if (clockHigh && !((i - 1) / half % 2 == 1))
            edges.push_back(i);
        v.process(false, false, 0.0f, clockHigh ? 10.0f : 0.0f);
        held.push_back(v.shOutVolts());
    }

    int changes = 0;
    for (size_t i = 1; i < held.size(); ++i)
    {
        if (held[i] != held[i - 1])
        {
            ++changes;
            // Every change must coincide with a rising edge.
            bool atEdge = false;
            for (size_t e : edges)
                atEdge |= e == i;
            CHECK(atEdge);
        }
        CHECK(std::abs(held[i]) <= 5.0f);
    }
    CHECK(changes >= 5); // the noise actually gets sampled
}

// ---------------------------------------------------------------- VCO A/B

namespace
{
std::vector<float> renderVco(const s42::VcoVoice::Params& p, float voctV, size_t n,
                             const float* sync = nullptr)
{
    s42::Tolerances tol;
    s42::VcoVoice v;
    v.setParams(p);
    v.prepare(kSr, tol, "vcoTest");

    std::vector<float> x(n);
    for (size_t i = 0; i < 4800; ++i) // settle the smoothers first
        v.process(voctV, 0.0f, 0.0f, sync != nullptr ? -5.0f : 0.0f);
    for (size_t i = 0; i < n; ++i)
        x[i] = v.process(voctV, 0.0f, 0.0f, sync != nullptr ? sync[i] : 0.0f) / 5.0f;
    return x;
}

double bandPeak(const std::vector<double>& mag, size_t center, size_t halfWidth)
{
    double peak = 0.0;
    for (size_t b = center - halfWidth; b <= center + halfWidth; ++b)
        peak = std::max(peak, mag[b]);
    return peak;
}
} // namespace

TEST_CASE("VCO -1 sub adds a strong f/2 component", "[engine][vco]")
{
    constexpr size_t n = 16384;
    constexpr size_t k0 = 256; // 750 Hz at 48 k
    const float voct = std::log2(750.0f / s42::kC0Hz) - s42::tuning::kVcoLowBaseV
                       - 0.5f; // minus the TUNE default half-octave

    s42::VcoVoice::Params p;
    p.wave01 = 0.6f; // sine/tri zone: clean spectrum around the sub
    const auto dry = testutil::spectrum(renderVco(p, voct, n));
    p.subOn = true;
    const auto sub = testutil::spectrum(renderVco(p, voct, n));

    const double dryHalf = bandPeak(dry, k0 / 2, 8);
    const double subHalf = bandPeak(sub, k0 / 2, 8);
    const double fund = bandPeak(sub, k0, 8);
    INFO("f/2: dry " << dryHalf << " -> sub " << subHalf << " (fund " << fund << ")");
    CHECK(subHalf > 10.0 * dryHalf);
    CHECK(subHalf > 0.2 * fund); // kVcoSubMix-strong, not residue
}

TEST_CASE("VCO A hard sync locks a detuned core to the master", "[engine][vco]")
{
    constexpr size_t n = 32768;
    constexpr size_t kMaster = 137; // 200.7 Hz, exact bin
    const double masterHz = (double) kMaster * kSr / (double) n;

    // Slave free-runs ~1.43x the master; saw shape shows sync teeth clearly.
    const float slaveHz = (float) masterHz * 1.43f;
    const float voct = std::log2(slaveHz / s42::kC0Hz) - s42::tuning::kVcoLowBaseV - 0.5f;

    std::vector<float> sync(n);
    double ph = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        ph += masterHz / kSr;
        if (ph >= 1.0)
            ph -= 1.0;
        sync[i] = ph < 0.5 ? 5.0f : -5.0f;
    }

    s42::VcoVoice::Params p; // wave 0 = saw
    const auto locked = testutil::spectrum(renderVco(p, voct, n, sync.data()));
    const auto free = testutil::spectrum(renderVco(p, voct, n));

    const double lockedAtMaster = bandPeak(locked, kMaster, 6);
    const double freeAtMaster = bandPeak(free, kMaster, 6);
    INFO("energy at master bin: locked " << lockedAtMaster << ", free " << freeAtMaster);
    CHECK(lockedAtMaster > 5.0 * freeAtMaster);
}

TEST_CASE("VCO morph rotary is finite and healthy across its whole travel", "[engine][vco]")
{
    for (float w : { 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f })
    {
        s42::VcoVoice::Params p;
        p.wave01 = w;
        const auto x = renderVco(p, 3.0f, 9600);

        double acc = 0.0;
        for (float v : x)
        {
            REQUIRE(std::isfinite(v));
            acc += (double) v * v;
        }
        const double rms = std::sqrt(acc / (double) x.size());
        INFO("wave " << w << " rms " << rms);
        CHECK(rms > 0.15); // alive
        CHECK(rms < 1.5);  // sane
    }
}

TEST_CASE("VCO square PW sets the duty cycle", "[engine][vco]")
{
    for (float pw : { 0.25f, 0.75f })
    {
        s42::VcoVoice::Params p;
        p.wave01 = 1.0f;
        p.pw01 = pw;
        const auto x = renderVco(p, 3.0f, 48000);

        size_t high = 0;
        for (float v : x)
            if (v > 0.0f)
                ++high;
        const double duty = (double) high / (double) x.size();
        INFO("pw " << pw << " duty " << duty);
        CHECK(duty == Approx(pw).margin(0.04));
    }
}

TEST_CASE("VCO square alias floor stays low with the shared kernel", "[engine][vco]")
{
    constexpr size_t n = 32768;
    constexpr size_t k0 = 683; // 1000.5 Hz
    const float f0 = (float) ((double) k0 * kSr / (double) n);

    // Cancel the per-unit 3340 trim tolerance so every harmonic lands exactly
    // on a k0 multiple — high harmonics of a detuned square would otherwise
    // drift whole bins off and read as false "aliases".
    s42::Tolerances tol;
    const float tolV = s42::tuning::kVcoToleranceCents
                       * tol.bipolar(s42::Tolerances::id("vcoTest")) / 1200.0f;
    const float voct = std::log2(f0 / s42::kC0Hz) - s42::tuning::kVcoLowBaseV - 0.5f - tolV;

    s42::VcoVoice::Params p;
    p.wave01 = 1.0f;
    const auto mag = testutil::spectrum(renderVco(p, voct, n));
    const double fund = bandPeak(mag, k0, 6);
    REQUIRE(fund > 0.0);

    const size_t audibleLimitBin = (size_t) (16000.0 * (double) n / kSr);
    double worstAudible = 0.0;
    for (size_t b = 16; b < audibleLimitBin; ++b)
    {
        const size_t dist = b % k0;
        if (dist <= 6 || dist >= k0 - 6)
            continue;
        worstAudible = std::max(worstAudible, mag[b]);
    }
    INFO("worst audible non-harmonic = " << testutil::db(worstAudible / fund) << " dBc");
    CHECK(testutil::db(worstAudible / fund) < -50.0);
}

// ---------------------------------------------------------------- photo sensor

TEST_CASE("photo sensor: fast brighten, slow LDR darken, room-light floor", "[dsp][sensor]")
{
    s42::PhotoSensor s;
    s.prepare(kSr);
    s.setRoom(0.0f, false);

    // LED full on: reaches most of the way in ~4 attack taus (20 ms).
    float v = 0.0f;
    for (int i = 0; i < (int) (0.02 * kSr); ++i)
        v = s.process(10.0f);
    CHECK(v > 9.0f);

    // LED off: after the same 20 ms the LDR barely darkened (120 ms memory).
    for (int i = 0; i < (int) (0.02 * kSr); ++i)
        v = s.process(0.0f);
    CHECK(v > 7.0f);

    // Negative CV never lights the LED.
    s42::PhotoSensor neg;
    neg.prepare(kSr);
    neg.setRoom(0.0f, false);
    float nv = 0.0f;
    for (int i = 0; i < 4800; ++i)
        nv = neg.process(-10.0f);
    CHECK(nv == Approx(0.0f).margin(1.0e-3));

    // Ambient room light alone drives the sensor.
    s42::PhotoSensor amb;
    amb.prepare(kSr);
    amb.setRoom(0.5f, false);
    float av = 0.0f;
    for (int i = 0; i < (int) kSr; ++i)
        av = amb.process(0.0f);
    CHECK(av == Approx(5.0f).margin(0.2));
}
