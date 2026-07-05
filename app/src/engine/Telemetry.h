#pragma once

#include <atomic>
#include <cstdint>

namespace s42 {

// Engine -> UI telemetry (plan §GUI): the audio thread publishes a small POD
// snapshot every sub-block (~750 Hz @48k); the UI samples it at 30-60 Hz for
// LEDs, photo-sensor glow and meters. Seqlock-guarded: the writer never
// blocks, the reader retries on a torn read. No locks, no allocation.
struct TelemetryData
{
    // Classic drones (index-aligned with Rack: DRONE 1, 2, 4, 5).
    float droneGen[4][5] = {}; // per-generator LED bar, 0..1
    float droneEnv[4] = {};    // AR envelope level, 0..1
    float sensor[4] = {};      // photo-sensor LDR brightness (window glow), 0..1

    // Papa Srapa (DRONE 3, 6).
    float srapaCv[2] = {};     // sub-audio modulator LED, 0..1
    float srapaEnv[2] = {};    // AR envelope level, 0..1

    float envA = 0.0f, envB = 0.0f; // ADSR levels, 0..1
    float lfoA = 0.0f, lfoB = 0.0f; // LFO outs, 0..1 (out/10 V)

    int32_t seqStep = 0;            // current sequencer stage, 0..4
    float seqGate = 0.0f;           // gate out high, 0/1
    float preampClip = 0.0f;        // preamp near rails this sub-block, 0/1
    float follower = 0.0f;          // env follower level, 0..1
    float followerGate = 0.0f;      // follower Schmitt gate, 0/1

    // Touch keyboard (M6).
    float kbGateL = 0.0f, kbGateR = 0.0f; // gate outs high, 0/1
    float kbVoct = 0.0f, kbPress = 0.0f;  // V/OCT + PRESSURE jack volts
    uint16_t kbHeld = 0;                  // held/latched plates, global bits
    int32_t kbStep[2] = { -1, -1 };       // arp/seq position per side, -1 = keyboard mode
    float kbExtClock = 0.0f;              // clocking from the CLOCK jack, 0/1
    int32_t kbOctave = 0;                 // single-behaviour octave shift
    float kbOffset[2] = {};               // transpose-offset toggles, 0/1

    float outL = 0.0f, outR = 0.0f; // master peak per sub-block, full-scale units
};

class Telemetry
{
public:
    void publish(const TelemetryData& d) noexcept
    {
        const uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_release); // odd = write in progress
        std::atomic_thread_fence(std::memory_order_release);
        data_ = d;
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s + 2, std::memory_order_release);
    }

    // False when every retry hit a concurrent write (caller keeps last good).
    bool read(TelemetryData& out) const noexcept
    {
        for (int tries = 0; tries < 8; ++tries)
        {
            const uint32_t a = seq_.load(std::memory_order_acquire);
            if ((a & 1u) != 0)
                continue;
            std::atomic_thread_fence(std::memory_order_acquire);
            out = data_;
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_acquire) == a)
                return true;
        }
        return false;
    }

private:
    std::atomic<uint32_t> seq_ { 0 };
    TelemetryData data_ {};
};

} // namespace s42
