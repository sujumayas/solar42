// M5: telemetry seqlock, jack-name lookups, panel jack census geometry, and
// the engine-side patching the CableLayer/PatchBay drive.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <thread>

#include "engine/Rack.h"
#include "engine/Telemetry.h"
#include "ui/PanelLayout.h"

TEST_CASE("Telemetry seqlock never yields a torn snapshot", "[m5][telemetry]")
{
    s42::Telemetry tel;
    std::atomic<bool> stop { false };
    std::atomic<int> torn { 0 }, good { 0 };

    // Writer: every field of a snapshot carries the same generation value.
    std::thread writer(
        [&]
        {
            s42::TelemetryData d;
            for (int gen = 0; !stop.load(std::memory_order_relaxed); ++gen)
            {
                const float v = (float) (gen & 0xffff);
                for (int a = 0; a < 4; ++a)
                {
                    for (int g = 0; g < 5; ++g)
                        d.droneGen[a][g] = v;
                    d.droneEnv[a] = v;
                    d.sensor[a] = v;
                }
                d.envA = d.envB = d.lfoA = d.lfoB = v;
                d.outL = d.outR = v;
                tel.publish(d);
            }
        });

    s42::TelemetryData r;
    for (int i = 0; i < 200000; ++i)
    {
        if (!tel.read(r))
            continue;
        const float v = r.envA;
        bool consistent = r.envB == v && r.lfoA == v && r.lfoB == v
                       && r.outL == v && r.outR == v;
        for (int a = 0; a < 4 && consistent; ++a)
            for (int g = 0; g < 5 && consistent; ++g)
                consistent = r.droneGen[a][g] == v && r.droneEnv[a] == v && r.sensor[a] == v;
        (consistent ? good : torn).fetch_add(1);
    }
    stop = true;
    writer.join();

    CHECK(torn.load() == 0);
    // The test writer publishes in a pathological tight loop (the real one
    // runs at ~750 Hz), so most reads legitimately give up — just prove the
    // reader does get through.
    CHECK(good.load() > 100);
}

TEST_CASE("Rack publishes telemetry while processing", "[m5][telemetry]")
{
    s42::Rack rack;
    rack.prepare(48000.0, 512);

    s42::Rack::Controls c;
    c.droneKey[0] = true;          // gate DRONE 1
    c.drone[0].mute[4] = true;     // and mute its gen 5
    rack.setControls(c);

    float outL[512], outR[512];
    for (int b = 0; b < 40; ++b)
        rack.process(outL, outR, nullptr, nullptr, 512);

    s42::TelemetryData t;
    REQUIRE(rack.telemetry().read(t));
    CHECK(t.droneEnv[0] > 0.5f);     // gated voice's AR env is up
    CHECK(t.droneGen[0][0] > 0.2f);  // unmuted gen LED lit
    CHECK(t.droneGen[0][4] < 0.05f); // muted gen LED dark
    CHECK(t.lfoA >= 0.0f);
    CHECK(t.lfoA <= 1.0f);
    CHECK(t.seqStep >= 0);
    CHECK(t.seqStep < 5);
}

TEST_CASE("Jack display-name lookups round-trip the registry", "[m5][jacks]")
{
    for (int i = 0; i < s42::kInletCount; ++i)
        CHECK(s42::inletIndexByName(s42::kInletInfo[i].name) == i);
    for (int o = 0; o < s42::kOutletCount; ++o)
        CHECK(s42::outletIndexByName(s42::kOutletInfo[o].name) == o);
    CHECK(s42::inletIndexByName("no.such.jack") == -1);
    CHECK(s42::outletIndexByName("vcoA.osc") == -1); // prefix must not match
}

TEST_CASE("Panel jack census geometry is sane", "[m5][layout]")
{
    using namespace solar::layout;

    // (Coverage — every inlet + every non-hidden outlet exactly once — is a
    // static_assert in PanelLayout.h; here: bounds and hit-target spacing.)
    for (const auto& j : kJacks)
    {
        CHECK(j.x > kJackR);
        CHECK(j.x < kPanelW - kJackR);
        CHECK(j.y > kJackR);
        CHECK(j.y < kPanelH - kJackR);
    }

    // Nearest-wins hit testing still needs jacks not to sit on top of each
    // other: minimum pairwise distance stays comfortably above the jack body.
    int minD2 = 1 << 30;
    for (int a = 0; a < kNumJacks; ++a)
        for (int b = a + 1; b < kNumJacks; ++b)
        {
            const int dx = kJacks[a].x - kJacks[b].x, dy = kJacks[a].y - kJacks[b].y;
            minD2 = std::min(minD2, dx * dx + dy * dy);
        }
    CHECK(minD2 >= 80 * 80);
}

TEST_CASE("Preamp ext-source jack overrides the host input", "[m5][routing]")
{
    s42::Rack rack;
    rack.prepare(48000.0, 256);

    s42::Rack::Controls c;
    c.preamp.gain = 1.0f;
    c.mixer.vol[6] = 1.0f; // PREAMP channel up
    rack.setControls(c);

    // Patch LFO A (unipolar 0..10 V) into the preamp and check the follower
    // env rises without any host input.
    rack.requestPatch(s42::Inlet::PreampExtIn, s42::Outlet::LfoAOut);
    float outL[256], outR[256];
    for (int b = 0; b < 100; ++b)
        rack.process(outL, outR, nullptr, nullptr, 256);

    s42::TelemetryData t;
    REQUIRE(rack.telemetry().read(t));
    CHECK(t.follower > 0.1f);

    // Unpatched (and still no host input) the follower decays back down.
    rack.requestUnpatch(s42::Inlet::PreampExtIn);
    for (int b = 0; b < 400; ++b)
        rack.process(outL, outR, nullptr, nullptr, 256);
    REQUIRE(rack.telemetry().read(t));
    CHECK(t.follower < 0.05f);
}

TEST_CASE("VCO dry outs carry the attenuated oscillators", "[m5][routing]")
{
    s42::Rack rack;
    rack.prepare(48000.0, 64);
    rack.setControls({});

    float outL[64], outR[64];
    for (int b = 0; b < 32; ++b)
        rack.process(outL, outR, nullptr, nullptr, 64);

    const float* osc = rack.bus().outRead(s42::Outlet::VcoBOscOut);
    const float* dry = rack.bus().outRead(s42::Outlet::VcoBDryOut);
    float peakOsc = 0.0f, worst = 0.0f;
    for (int i = 0; i < 64; ++i)
    {
        peakOsc = std::max(peakOsc, std::abs(osc[i]));
        worst = std::max(worst, std::abs(dry[i] - osc[i] * 0.2f));
    }
    CHECK(peakOsc > 1.0f); // the VCO is actually running
    CHECK(worst < 1e-6f);  // spec p4: ~5 V osc -> max 1 V dry out
}
