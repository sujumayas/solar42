// M9c P2 — the malloc guard from the plan's Verification list: global
// new/delete are replaced binary-wide with counting hooks; while "armed"
// any allocation or free is a violation. A busy full rig (every voice
// sounding, keyboard touches, patch commands draining mid-run, an FV-1
// program latch mid-run) must process with ZERO hits — the same claim the
// audio thread relies on every block.
//
// Scope note: this guards the JUCE-free engine (Rack::process +
// setControls, the exact per-block path). The processor layer above it is
// audited by hand (see 08-implementation-plan.md M9c P2): param() reads a
// pre-resolved table, id strings build in stack buffers, and MIDI parses
// raw bytes — no juce::String or MidiMessage is constructed per block.

#include "engine/Rack.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

namespace rtguard
{
std::atomic<bool> armed { false };
std::atomic<long> hits { 0 };

inline void note() noexcept
{
    if (armed.load(std::memory_order_relaxed))
        hits.fetch_add(1, std::memory_order_relaxed);
}

struct Arm
{
    Arm() { hits.store(0); armed.store(true); }
    ~Arm() { armed.store(false); }
};
} // namespace rtguard

// Canonical replaceable allocation functions (C++17 set). Deletes count
// too: free() is as forbidden on the audio thread as malloc().
void* operator new(std::size_t sz)
{
    rtguard::note();
    if (void* p = std::malloc(sz > 0 ? sz : 1))
        return p;
    throw std::bad_alloc {};
}
void* operator new[](std::size_t sz) { return ::operator new(sz); }
void* operator new(std::size_t sz, const std::nothrow_t&) noexcept
{
    rtguard::note();
    return std::malloc(sz > 0 ? sz : 1);
}
void* operator new[](std::size_t sz, const std::nothrow_t& t) noexcept
{
    return ::operator new(sz, t);
}
void* operator new(std::size_t sz, std::align_val_t al)
{
    rtguard::note();
    void* p = nullptr;
    if (posix_memalign(&p, (size_t) al, sz > 0 ? sz : 1) == 0)
        return p;
    throw std::bad_alloc {};
}
void* operator new[](std::size_t sz, std::align_val_t al) { return ::operator new(sz, al); }

void operator delete(void* p) noexcept
{
    rtguard::note();
    std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete(void* p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }

namespace
{
s42::Rack::Controls busyControls()
{
    s42::Rack::Controls c;
    for (int v = 0; v < s42::Rack::kClassicVoices; ++v)
        c.droneKey[v] = true;
    for (int s = 0; s < s42::Rack::kSrapaVoices; ++s)
    {
        c.srapaKey[s] = true;
        c.srapa[s].fmOn = true;
        c.srapa[s].noise01 = 0.6f;
    }
    c.envA.hold = true;
    c.envB.hold = true;
    c.kbTouch.plate[0] = 0.9f; // keyboard voice sounding through the VCOs
    c.kbTouch.plate[7] = 0.5f;
    c.fx.blend = 0.8f;         // both FV-1 chips fully in the path
    c.fx.loadInserted();
    c.preamp.gain = 3.0f;
    return c;
}
} // namespace

TEST_CASE("audio thread allocates nothing (malloc guard over the full rig)", "[rt]")
{
    constexpr int kBlock = 512;
    auto rack = std::make_unique<s42::Rack>();
    rack->prepare(48000.0, kBlock); // assembly/lazy init happens HERE, unarmed

    auto c = busyControls();
    rack->setControls(c);

    float outL[kBlock], outR[kBlock];
    float extL[kBlock] = {}, extR[kBlock] = {};
    extL[0] = 1.0f; // give the preamp/follower an edge to chew on

    {
        rtguard::Arm guard;
        for (int block = 0; block < 200; ++block)
        {
            // Mirror the per-block processor behaviour: fresh controls every
            // block, patch edits and an FV-1 program latch arriving mid-run.
            if (block == 20)
            {
                REQUIRE(rack->requestPatch(s42::Inlet::FiltCvLIn, s42::Outlet::LfoAOut));
                REQUIRE(rack->requestPatch(s42::Inlet::SeqClockIn, s42::Outlet::LfoBOut));
            }
            if (block == 60)
                c.fx.loadedProgL = 1; // program latch: loadProgram on the audio thread
            if (block == 120)
                REQUIRE(rack->requestUnpatch(s42::Inlet::FiltCvLIn));

            rack->setControls(c);
            rack->process(outL, outR, extL, extR, kBlock);
        }
    }

    REQUIRE(rtguard::hits.load() == 0);

    // The guard itself must be live, or the zero above proves nothing.
    // (Direct calls — a `delete new int` pair is elidable since C++14.)
    {
        rtguard::Arm guard;
        void* p = ::operator new(4);
        ::operator delete(p);
    }
    REQUIRE(rtguard::hits.load() == 2);
}
