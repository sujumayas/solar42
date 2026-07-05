#include <catch2/catch_test_macros.hpp>

#include "engine/Rack.h"
#include "engine/VoltBus.h"

#include <cmath>
#include <random>
#include <vector>

using namespace s42;

TEST_CASE("open inlets read 0 V; patched inlets read the outlet", "[engine][routing]")
{
    VoltBus bus;
    bus.out(Outlet::LfoAOut)[0] = 7.5f;

    CHECK(bus.in(Inlet::D1CvIn)[0] == 0.0f);
    CHECK_FALSE(bus.isConnected(Inlet::D1CvIn));

    bus.patch(Inlet::D1CvIn, Outlet::LfoAOut);
    CHECK(bus.in(Inlet::D1CvIn)[0] == 7.5f);
    CHECK(bus.isPatched(Inlet::D1CvIn));

    bus.unpatch(Inlet::D1CvIn);
    CHECK(bus.in(Inlet::D1CvIn)[0] == 0.0f);
}

TEST_CASE("hardware normals connect by default and break when patched", "[engine][routing]")
{
    VoltBus bus;
    bus.out(Outlet::KbVoctOut)[0] = 3.0f;
    bus.out(Outlet::SeqCvOut)[0] = 1.0f;

    // Keyboard V/OCT is normalled to both VCO 1v/oct inputs.
    CHECK(bus.in(Inlet::VcoAVoctIn)[0] == 3.0f);
    CHECK(bus.in(Inlet::VcoBVoctIn)[0] == 3.0f);
    CHECK_FALSE(bus.isPatched(Inlet::VcoAVoctIn)); // normal, not a cable

    // Patching the sequencer into VCO A breaks only VCO A's normal.
    bus.patch(Inlet::VcoAVoctIn, Outlet::SeqCvOut);
    CHECK(bus.in(Inlet::VcoAVoctIn)[0] == 1.0f);
    CHECK(bus.in(Inlet::VcoBVoctIn)[0] == 3.0f);

    bus.unpatch(Inlet::VcoAVoctIn);
    CHECK(bus.in(Inlet::VcoAVoctIn)[0] == 3.0f);

    // VCO A's internal output is normalled to VCO B's CV (the 2-op FM path).
    bus.out(Outlet::VcoAOscInt)[0] = -2.0f;
    CHECK(bus.in(Inlet::VcoBCvIn)[0] == -2.0f);
}

TEST_CASE("42N filter CV R follows whatever is patched into CV L", "[engine][routing]")
{
    VoltBus bus;
    bus.out(Outlet::LfoBOut)[0] = 4.0f;
    bus.out(Outlet::JoyXOut)[0] = -6.0f;

    CHECK(bus.in(Inlet::FiltCvRIn)[0] == 0.0f); // nothing in CV L yet

    bus.patch(Inlet::FiltCvLIn, Outlet::LfoBOut);
    CHECK(bus.in(Inlet::FiltCvRIn)[0] == 4.0f); // follows CV L's cable

    bus.patch(Inlet::FiltCvRIn, Outlet::JoyXOut); // own cable wins
    CHECK(bus.in(Inlet::FiltCvRIn)[0] == -6.0f);

    bus.unpatch(Inlet::FiltCvRIn);
    CHECK(bus.in(Inlet::FiltCvRIn)[0] == 4.0f);

    bus.unpatch(Inlet::FiltCvLIn);
    CHECK(bus.in(Inlet::FiltCvRIn)[0] == 0.0f);
}

TEST_CASE("LFO A modulates a drone generator through the patch bus", "[engine][routing]")
{
    Rack rack;
    rack.prepare(48000.0, 512);

    Rack::Controls c;
    c.drone[0].hold = true;
    c.drone[0].attackSec = 0.001f;
    for (int g = 1; g < 5; ++g)
        c.drone[0].mute[g] = true;
    c.drone[0].mod[0] = true;    // gen 1 listens to the CV bus
    c.drone[0].tune[0] = 0.8f;
    c.roomLight = 0.0f;          // keep the photo-sensor dark: CV only
    c.lfoA.rateHz = 4.0f;
    c.lfoA.wave = 1.0f;          // triangle
    rack.setControls(c);

    auto render = [&](bool patched)
    {
        if (patched)
            rack.requestPatch(Inlet::D1CvIn, Outlet::LfoAOut);
        else
            rack.requestUnpatch(Inlet::D1CvIn);
        std::vector<float> l(48000), r(48000);
        for (int i = 0; i < 48000; i += 512)
            rack.process(l.data() + i, r.data() + i, nullptr, nullptr, 512);
        return l;
    };

    const auto dry = render(false);
    const auto wet = render(true);

    // With the LFO patched the pitch wobbles: the two renders diverge hard.
    double diff = 0.0;
    for (size_t i = 24000; i < dry.size(); ++i)
        diff += std::abs((double) dry[i] - (double) wet[i]);
    CHECK(diff / 24000.0 > 0.001);
}

TEST_CASE("cables survive a re-prepare (power cycle must not unplug)", "[engine][routing]")
{
    // Hosts call prepare on every transport/device/buffer change; the engine
    // wiping its routing there left cables dead while the UI still drew them
    // (found 2026-07-05 via a silent calibration render).
    Rack rack;
    rack.prepare(48000.0, 512);

    Rack::Controls c;
    c.seq.pulserHz = 8.0f;
    rack.setControls(c);
    rack.requestPatch(Inlet::VcoAVoctIn, Outlet::SeqCvOut);

    std::vector<float> l(512), r(512);
    rack.process(l.data(), r.data(), nullptr, nullptr, 512); // drain the patch
    REQUIRE(rack.bus().isPatched(Inlet::VcoAVoctIn));

    rack.prepare(48000.0, 512); // sample-rate / device change
    rack.process(l.data(), r.data(), nullptr, nullptr, 512);
    CHECK(rack.bus().isPatched(Inlet::VcoAVoctIn));

    // And the patched signal actually flows again after the re-prepare.
    CHECK(rack.bus().in(Inlet::VcoAVoctIn) == rack.bus().outRead(Outlet::SeqCvOut));
}

TEST_CASE("feedback patch (env out -> own CV) stays bounded", "[engine][routing]")
{
    Rack rack;
    rack.prepare(48000.0, 512);

    Rack::Controls c;
    c.drone[0].hold = true;
    c.drone[0].attackSec = 0.01f;
    for (int g = 0; g < 5; ++g)
        c.drone[0].mod[g] = true; // every generator eats the feedback
    rack.setControls(c);
    rack.requestPatch(Inlet::D1CvIn, Outlet::D1EnvOut);

    std::vector<float> l(96000), r(96000);
    for (int i = 0; i < 96000; i += 512)
        rack.process(l.data() + i, r.data() + i, nullptr, nullptr, 512);

    for (float v : l)
        REQUIRE(std::isfinite(v));
    for (size_t i = 48000; i < l.size(); ++i)
        REQUIRE(std::abs(l[i]) < 1.5f); // saturates musically, no blow-up
}

TEST_CASE("random patch storm while rendering never produces NaN", "[engine][routing]")
{
    Rack rack;
    rack.prepare(48000.0, 512);

    Rack::Controls c;
    c.drone[0].hold = true;
    for (int g = 0; g < 5; ++g)
        c.drone[0].mod[g] = true;
    rack.setControls(c);

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> inletDist(0, kInletCount - 1);
    std::uniform_int_distribution<int> outletDist(0, kOutletCount - 1);
    std::uniform_int_distribution<int> opDist(0, 2);

    std::vector<float> l(512), r(512);
    for (int block = 0; block < 400; ++block)
    {
        for (int e = 0; e < 8; ++e)
        {
            const auto inlet = (Inlet) inletDist(rng);
            if (opDist(rng) == 0)
                rack.requestUnpatch(inlet);
            else
                rack.requestPatch(inlet, (Outlet) outletDist(rng));
        }
        rack.process(l.data(), r.data(), nullptr, nullptr, 512);
        for (float v : l)
            REQUIRE(std::isfinite(v));
    }
}

TEST_CASE("LFO output is unipolar 0..10 V and tracks its rate", "[engine][modstrip]")
{
    Rack rack;
    rack.prepare(48000.0, 512);
    Rack::Controls c;
    c.lfoA.rateHz = 5.0f;
    c.lfoA.wave = 0.1f; // near-square
    rack.setControls(c);
    rack.requestPatch(Inlet::D1CvIn, Outlet::LfoAOut); // to observe via bus

    std::vector<float> l(48000), r(48000);
    std::vector<float> lfo;
    for (int i = 0; i < 48000; i += 512)
    {
        rack.process(l.data() + i, r.data() + i, nullptr, nullptr, 512);
        const float* buf = rack.bus().in(Inlet::D1CvIn);
        for (int k = 0; k < VoltBus::kSubBlock; ++k)
            lfo.push_back(buf[k]); // last sub-block's worth per call is fine
    }

    float mn = 1e9f, mx = -1e9f;
    for (float v : lfo)
    {
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    CHECK(mn >= 0.0f);       // unipolar by design (photo-LED drive)
    CHECK(mx <= 10.001f);
    CHECK(mx > 9.0f);

    int rises = 0;
    for (size_t i = 1; i < lfo.size(); ++i)
        if (lfo[i - 1] < 5.0f && lfo[i] >= 5.0f)
            ++rises;
    CHECK(rises >= 4);
    CHECK(rises <= 6);
}

TEST_CASE("sequencer steps and wraps at STAGES", "[engine][modstrip]")
{
    Rack rack;
    rack.prepare(48000.0, 512);
    Rack::Controls c;
    c.seq.pulserHz = 100.0f;
    c.seq.stages = 3;
    c.seq.cv[0] = 1.0f;
    c.seq.cv[1] = 2.0f;
    c.seq.cv[2] = 3.0f;
    c.seq.cv[3] = 4.0f; // must never appear (stages = 3)
    rack.setControls(c);
    rack.requestPatch(Inlet::D1CvIn, Outlet::SeqCvOut);

    std::vector<float> l(48000), r(48000);
    std::vector<float> seen;
    for (int i = 0; i < 48000; i += 512)
    {
        rack.process(l.data() + i, r.data() + i, nullptr, nullptr, 512);
        const float* buf = rack.bus().in(Inlet::D1CvIn);
        for (int k = 0; k < VoltBus::kSubBlock; ++k)
            seen.push_back(buf[k]);
    }

    bool saw1 = false, saw2 = false, saw3 = false, saw4 = false;
    for (float v : seen)
    {
        saw1 |= std::abs(v - 1.0f) < 0.01f;
        saw2 |= std::abs(v - 2.0f) < 0.01f;
        saw3 |= std::abs(v - 3.0f) < 0.01f;
        saw4 |= std::abs(v - 4.0f) < 0.01f;
    }
    CHECK(saw1);
    CHECK(saw2);
    CHECK(saw3);
    CHECK_FALSE(saw4);
}
