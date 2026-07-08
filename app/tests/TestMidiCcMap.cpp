// M9c P5 — MidiCcMap: the CC-learn table behind the right-click menu.
// Audio side pushes raw controller events; the message-thread drain either
// consumes the first learnable event as a binding or dispatches value01.

#include "engine/MidiCcMap.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using s42::MidiCcMap;

namespace
{
std::vector<std::pair<int, float>> drained(MidiCcMap& m)
{
    std::vector<std::pair<int, float>> out;
    m.drain([&](int slot, float v) { out.emplace_back(slot, v); });
    return out;
}
} // namespace

TEST_CASE("cc learn binds the first learnable event and consumes it", "[ccmap]")
{
    MidiCcMap m;
    m.armLearn(7);
    REQUIRE(m.armedSlot() == 7);

    m.onCcFromAudio(74, 100);   // the learn gesture
    m.onCcFromAudio(74, 90);    // follow-through: must dispatch
    const auto got = drained(m);

    REQUIRE(m.armedSlot() == MidiCcMap::kUnmapped);
    REQUIRE(m.ccForSlot(7) == 74);
    REQUIRE(got.size() == 1);
    REQUIRE(got[0].first == 7);
    REQUIRE(got[0].second > 0.70f);
    REQUIRE(got[0].second < 0.72f); // 90/127
}

TEST_CASE("one cc per param and one param per cc", "[ccmap]")
{
    MidiCcMap m;
    m.bind(20, 3);
    m.bind(21, 3); // re-learn moves the binding off cc 20
    REQUIRE(m.slotForCc(20) == MidiCcMap::kUnmapped);
    REQUIRE(m.ccForSlot(3) == 21);

    m.bind(21, 9); // rebinding the cc retargets it
    REQUIRE(m.slotForCc(21) == 9);
    REQUIRE(m.ccForSlot(3) == MidiCcMap::kUnmapped);
}

TEST_CASE("reserved ccs never learn or dispatch", "[ccmap]")
{
    MidiCcMap m;
    m.armLearn(4);
    m.onCcFromAudio(64, 127);  // sustain — reserved
    m.onCcFromAudio(123, 0);   // channel mode — reserved
    REQUIRE(drained(m).empty());
    REQUIRE(m.armedSlot() == 4); // still waiting for a learnable cc

    m.bind(64, 4);  // programmatic binds of reserved ccs are refused too
    m.bind(123, 4);
    REQUIRE(m.ccForSlot(4) == MidiCcMap::kUnmapped);
}

TEST_CASE("unbound ccs are silent; clear works", "[ccmap]")
{
    MidiCcMap m;
    m.onCcFromAudio(30, 64);
    REQUIRE(drained(m).empty());

    m.bind(30, 11);
    m.onCcFromAudio(30, 127);
    auto got = drained(m);
    REQUIRE(got.size() == 1);
    REQUIRE(got[0] == std::pair<int, float>(11, 1.0f));

    m.clearSlot(11);
    m.onCcFromAudio(30, 127);
    REQUIRE(drained(m).empty());

    m.bind(31, 2);
    m.clearAll();
    REQUIRE(m.ccForSlot(2) == MidiCcMap::kUnmapped);
}

TEST_CASE("cancel learn leaves the map untouched", "[ccmap]")
{
    MidiCcMap m;
    m.bind(50, 5);
    m.armLearn(6);
    m.cancelLearn();
    m.onCcFromAudio(51, 10);
    REQUIRE(drained(m).empty()); // 51 never bound
    REQUIRE(m.ccForSlot(5) == 50);
    REQUIRE(m.ccForSlot(6) == MidiCcMap::kUnmapped);
}
