#pragma once

#include <cstdint>

namespace s42 {

// The "live stereo" mechanism: a deterministic per-unit component-tolerance
// generator. Seeded by a persisted "unit serial" saved with the rig state, so a
// patch always sounds like *your* unit; re-seeding = swapping hardware units.
// Every L/R-paired or multi-instance element derives its fixed offsets here
// (filter params +-2-3 %, FV-1 clocks +-0.05 %, generator cents, VCA gains...).
class Tolerances
{
public:
    explicit Tolerances(uint64_t unitSerial = 0x50DA842Aull) : serial_(unitSerial) {}

    void reseed(uint64_t unitSerial) noexcept { serial_ = unitSerial; }
    uint64_t serial() const noexcept { return serial_; }

    // Fixed value in [-1, 1) for a named component, stable for the unit's lifetime.
    float bipolar(uint64_t componentId) const noexcept
    {
        const uint64_t bits = splitmix(serial_ ^ componentId) >> 11; // 53 random bits
        return (float) ((double) bits * (1.0 / 4503599627370496.0) - 1.0); // [0,2)->[-1,1)
    }

    // Fixed multiplier 1 +- magnitude for a named component.
    float spread(uint64_t componentId, float magnitude) const noexcept
    {
        return 1.0f + magnitude * bipolar(componentId);
    }

    // Component ids are FNV-1a hashes of stable name strings, e.g. "filtL.freq".
    static constexpr uint64_t id(const char* name) noexcept
    {
        uint64_t h = 1469598103934665603ull;
        while (*name != '\0')
        {
            h ^= (uint64_t) (unsigned char) *name++;
            h *= 1099511628211ull;
        }
        return h;
    }

private:
    static constexpr uint64_t splitmix(uint64_t x) noexcept
    {
        x += 0x9E3779B97f4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    uint64_t serial_;
};

} // namespace s42
