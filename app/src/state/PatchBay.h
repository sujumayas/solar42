#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/Jacks.h"
#include "engine/Rack.h"

namespace solar {

// The message-thread canonical cable list (plan §engine architecture): a
// CABLES child of the APVTS state tree, one CABLE {in, out} child per cable,
// jacks stored by display name so saved states survive registry appends.
// Every mutation pushes the matching command into the Rack's lock-free queue;
// the UI listens for change messages to repaint the CableLayer.
//
// One cable per inlet (hardware jack); many cables per outlet (built-in mult).
class PatchBay : public juce::ChangeBroadcaster
{
public:
    PatchBay(juce::ValueTree& rootState, s42::Rack& rack) : root_(rootState), rack_(rack) {}

    // Bind to the (possibly replaced) state tree and push the full cable list
    // into the engine. Call once after construction and after replaceState.
    void rebind()
    {
        cables_ = root_.getOrCreateChildWithName("CABLES", nullptr);

        // Drop children that no longer resolve (or duplicate an inlet).
        for (int i = cables_.getNumChildren(); --i >= 0;)
        {
            const auto c = cables_.getChild(i);
            const int in = s42::inletIndexByName(c["in"].toString().toRawUTF8());
            const int out = s42::outletIndexByName(c["out"].toString().toRawUTF8());
            if (in < 0 || out < 0 || indexOfCable((s42::Inlet) in) != i)
                cables_.removeChild(i, nullptr);
        }

        for (int i = 0; i < s42::kInletCount; ++i)
        {
            const auto src = sourceOf((s42::Inlet) i);
            if (src >= 0)
                rack_.requestPatch((s42::Inlet) i, (s42::Outlet) src);
            else
                rack_.requestUnpatch((s42::Inlet) i);
        }
        sendChangeMessage();
    }

    void connect(s42::Inlet in, s42::Outlet out)
    {
        auto c = cableFor(in);
        if (!c.isValid())
        {
            c = juce::ValueTree("CABLE");
            c.setProperty("in", juce::String(s42::kInletInfo[(int) in].name), nullptr);
            cables_.appendChild(c, nullptr);
        }
        c.setProperty("out", juce::String(s42::kOutletInfo[(int) out].name), nullptr);
        rack_.requestPatch(in, out);
        sendChangeMessage();
    }

    void disconnect(s42::Inlet in)
    {
        const int i = indexOfCable(in);
        if (i >= 0)
            cables_.removeChild(i, nullptr);
        rack_.requestUnpatch(in);
        sendChangeMessage();
    }

    // Outlet index feeding this inlet's jack, or -1 when no cable.
    int sourceOf(s42::Inlet in) const
    {
        const auto c = cableFor(in);
        return c.isValid() ? s42::outletIndexByName(c["out"].toString().toRawUTF8()) : -1;
    }

private:
    int indexOfCable(s42::Inlet in) const
    {
        const juce::String name(s42::kInletInfo[(int) in].name);
        for (int i = 0; i < cables_.getNumChildren(); ++i)
            if (cables_.getChild(i)["in"].toString() == name)
                return i;
        return -1;
    }

    juce::ValueTree cableFor(s42::Inlet in) const
    {
        const int i = indexOfCable(in);
        return i >= 0 ? cables_.getChild(i) : juce::ValueTree();
    }

    juce::ValueTree& root_;
    juce::ValueTree cables_;
    s42::Rack& rack_;
};

} // namespace solar
