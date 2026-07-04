#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace testutil {

// In-place radix-2 FFT, N a power of two. Plenty for test fixtures.
inline void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Blackman-Harris(4)-windowed magnitude spectrum (first N/2 bins). BH4's
// -92 dB sidelobes let tests measure alias floors below -60 dBc (Hann's
// -31 dB sidelobes leak ~-50 dBc skirts around strong harmonics).
inline std::vector<double> spectrum(const std::vector<float>& x)
{
    const size_t n = x.size();
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * M_PI * (double) i / (double) (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2.0 * t)
                         - 0.01168 * std::cos(3.0 * t);
        a[i] = x[i] * w;
    }
    fft(a);
    std::vector<double> mag(n / 2);
    for (size_t i = 0; i < n / 2; ++i)
        mag[i] = std::abs(a[i]);
    return mag;
}

inline double db(double ratio) { return 20.0 * std::log10(ratio); }

// Robust oscillation-cycle counter: hysteresis around +-threshold, immune to
// the 2-sample smear polyBLEP puts on a saw reset.
inline int countCycles(const std::vector<float>& x, float threshold)
{
    int cycles = 0;
    bool armed = false;
    for (float v : x)
    {
        if (v < -threshold)
            armed = true;
        else if (armed && v > threshold)
        {
            ++cycles;
            armed = false;
        }
    }
    return cycles;
}

} // namespace testutil
