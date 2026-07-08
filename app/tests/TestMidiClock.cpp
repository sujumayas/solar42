// M9c P4 — MidiClockSource: 24-ppqn real-time ticks -> divided 10 V pulse
// train on the MIDI CLK jack. Sample-accurate placement, 50 % duty measured
// pulse-to-pulse, START/CONTINUE phase reset, STOP mute, free-run default.

#include "engine/MidiClockSource.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using s42::MidiClockFeed;
using s42::MidiClockSource;

namespace
{
// Run one host block the way the Rack does: beginBlock once, then
// 64-sample sub-blocks with block-relative offsets.
std::vector<float> runBlock(MidiClockSource& src, const MidiClockFeed& f, int blockLen)
{
    std::vector<float> out((size_t) blockLen, -1.0f);
    src.beginBlock(f);
    for (int off = 0; off < blockLen; off += 64)
        src.process(out.data() + off, off, std::min(64, blockLen - off));
    return out;
}

MidiClockFeed ticksEvery(int spacing, int count, int firstAt = 0)
{
    MidiClockFeed f;
    for (int i = 0; i < count && i < MidiClockFeed::kMaxTicks; ++i)
        f.tickAt[f.numTicks++] = firstAt + i * spacing;
    return f;
}

std::vector<int> risingEdges(const std::vector<float>& v)
{
    std::vector<int> e;
    float prev = 0.0f;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (v[i] > 5.0f && prev <= 5.0f)
            e.push_back((int) i);
        prev = v[i];
    }
    return e;
}
} // namespace

TEST_CASE("midi clock divides ticks into sample-accurate pulses", "[midiclock]")
{
    MidiClockSource src;
    src.prepare(48000.0);
    src.setDivider(6); // 1/16 at 24 ppqn

    // 48 ticks 100 samples apart -> a pulse every 6th tick = every 600 samples.
    const auto out = runBlock(src, ticksEvery(100, 48), 4800);
    const auto edges = risingEdges(out);
    REQUIRE(edges == std::vector<int> { 0, 600, 1200, 1800, 2400, 3000, 3600, 4200 });

    // 50 % duty once an interval has been measured: the second pulse holds
    // 300 samples (600 / 2) then falls.
    REQUIRE(out[899] > 5.0f);
    REQUIRE(out[900] < 1.0f);
}

TEST_CASE("midi clock phase continues across host blocks", "[midiclock]")
{
    MidiClockSource src;
    src.prepare(48000.0);
    src.setDivider(6);

    // 4 ticks in block one (pulse on global tick 0), 8 in block two: the
    // next divided pulse is global tick 6 = block two's third tick at
    // offset 200 — the divider phase carries across blocks, no reset.
    // (Block one is 600 long so the 10 ms first-pulse tail ends inside it.)
    (void) runBlock(src, ticksEvery(100, 4), 600);
    const auto out = runBlock(src, ticksEvery(100, 8), 800);
    REQUIRE(risingEdges(out) == std::vector<int> { 200 });
}

TEST_CASE("midi clock STOP mutes, START resets the divider phase", "[midiclock]")
{
    MidiClockSource src;
    src.prepare(48000.0);
    src.setDivider(6);

    (void) runBlock(src, ticksEvery(100, 3), 300); // mid-cycle: 3 of 6 ticks in

    auto stopped = ticksEvery(100, 6);
    stopped.stop = true;
    REQUIRE(risingEdges(runBlock(src, stopped, 600)).empty()); // ticks ignored

    auto resumed = ticksEvery(100, 7, 50);
    resumed.start = true; // phase reset: the very next tick pulses
    const auto edges = risingEdges(runBlock(src, resumed, 800));
    REQUIRE(edges.size() == 2);
    REQUIRE(edges[0] == 50);
    REQUIRE(edges[1] == 650);
}

TEST_CASE("midi clock free-runs without a START (hardware clocks)", "[midiclock]")
{
    MidiClockSource src;
    src.prepare(48000.0);
    src.setDivider(3); // 1/32

    const auto edges = risingEdges(runBlock(src, ticksEvery(200, 9), 1800));
    REQUIRE(edges == std::vector<int> { 0, 600, 1200 });
}
