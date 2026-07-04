#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace s42 {

// Streaming arbitrary-ratio resampler for the host-rate <-> 32,768 Hz FV-1
// boundary. Kaiser-windowed sinc prototype (48 input-spaced taps, beta 10)
// sampled through a 256-phase polyphase table with linear phase
// interpolation. Cutoff sits at 0.45x the smaller rate, so the ~15 kHz
// band edge of the real chip's converters falls out naturally; per-channel
// ratio skew (the two-chips-in-one-box "live stereo" trick) is just a
// slightly different outRate. Allocation happens in prepare() only.
class PolyphaseResampler
{
public:
    static constexpr int kTaps = 48;
    static constexpr int kPhases = 256;

    void prepare(double inRate, double outRate)
    {
        step_ = inRate / outRate;
        buildTable(0.45 * std::min(inRate, outRate) / inRate);
        reset();
    }

    void reset() noexcept
    {
        std::fill(hist_.begin(), hist_.end(), 0.0f);
        received_ = 0;
        outInt_ = 0;
        outFrac_ = 0.0;
    }

    // Group delay in input samples (for tests; the engine just lets it ring).
    static constexpr double latencyInSamples() { return kTaps / 2.0; }

    // Push one input sample; writes 0..maxOut resampled samples to out.
    int push(float x, float* out, int maxOut) noexcept
    {
        hist_[(size_t) (received_ & kMask)] = x;
        ++received_;

        int produced = 0;
        // Output at position p needs inputs up to floor(p) + kTaps/2.
        while (produced < maxOut && outInt_ + kTaps / 2 <= received_ - 1)
        {
            out[produced++] = interpolate();
            outFrac_ += step_;
            const auto carry = (int64_t) outFrac_;
            outInt_ += carry;
            outFrac_ -= (double) carry;
        }
        return produced;
    }

private:
    static constexpr int kRing = 128; // > kTaps, power of two
    static constexpr int64_t kMask = kRing - 1;

    static double besselI0(double x) noexcept
    {
        // Series expansion; converges quickly for the beta range used here.
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-18 * sum)
                break;
        }
        return sum;
    }

    void buildTable(double fcNorm)
    {
        constexpr double kBeta = 10.0;
        table_.assign((size_t) ((kPhases + 1) * kTaps), 0.0f);
        const double i0b = besselI0(kBeta);
        const double half = kTaps / 2.0;

        for (int p = 0; p <= kPhases; ++p)
        {
            const double frac = (double) p / (double) kPhases;
            double sum = 0.0;
            double row[kTaps];
            for (int k = 0; k < kTaps; ++k)
            {
                // tap k weights x[floor(pos) - kTaps/2 + 1 + k]
                const double t = frac + half - 1.0 - (double) k;
                const double x = t / half;
                double w = 0.0;
                if (x > -1.0 && x < 1.0)
                {
                    const double kaiser = besselI0(kBeta * std::sqrt(1.0 - x * x)) / i0b;
                    const double arg = 2.0 * fcNorm * t;
                    const double sinc = arg == 0.0 ? 1.0
                                                   : std::sin(3.141592653589793 * arg)
                                                         / (3.141592653589793 * arg);
                    w = 2.0 * fcNorm * sinc * kaiser;
                }
                row[k] = w;
                sum += w;
            }
            // Per-phase DC normalization keeps the passband exactly flat at 0 Hz.
            for (int k = 0; k < kTaps; ++k)
                table_[(size_t) (p * kTaps + k)] = (float) (row[k] / sum);
        }
        hist_.assign(kRing, 0.0f);
    }

    float interpolate() const noexcept
    {
        const double scaled = outFrac_ * kPhases;
        const int p0 = (int) scaled;
        const float mix = (float) (scaled - p0);
        const float* a = &table_[(size_t) (p0 * kTaps)];
        const float* b = a + kTaps;
        const int64_t j0 = outInt_ - kTaps / 2 + 1;

        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k)
        {
            const float w = a[k] + mix * (b[k] - a[k]);
            acc += w * hist_[(size_t) ((j0 + k) & kMask)];
        }
        return acc;
    }

    std::vector<float> table_;
    std::vector<float> hist_;
    double step_ = 1.0;
    int64_t received_ = 0;
    int64_t outInt_ = 0;
    double outFrac_ = 0.0;
};

} // namespace s42
