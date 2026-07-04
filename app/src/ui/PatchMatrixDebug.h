#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "engine/Jacks.h"
#include "engine/Rack.h"

// M2 debug patching UI: one combo per panel inlet listing every panel outlet.
// Replaced by the skeuomorphic CableLayer in M5 — this exists so routing can
// be exercised and heard before any panel art exists.
class PatchMatrixDebug : public juce::Component
{
public:
    explicit PatchMatrixDebug(s42::Rack& rack) : rack_(rack)
    {
        for (int i = 0; i < s42::kInletCount; ++i)
        {
            auto& info = s42::kInletInfo[(size_t) i];

            auto label = std::make_unique<juce::Label>();
            label->setText(info.name, juce::dontSendNotification);
            label->setFont(juce::FontOptions(12.0f));
            addAndMakeVisible(*label);
            labels_.add(label.release());

            auto box = std::make_unique<juce::ComboBox>();
            box->addItem(juce::String::fromUTF8("—"), 1); // em dash = unpatched
            for (int o = 0; o < s42::kOutletCount; ++o)
                if (!s42::kOutletInfo[(size_t) o].hidden)
                    box->addItem(s42::kOutletInfo[(size_t) o].name, o + 2);
            box->setSelectedId(1, juce::dontSendNotification);

            const auto inlet = (s42::Inlet) i;
            box->onChange = [this, inlet, b = box.get()]
            {
                const int id = b->getSelectedId();
                if (id <= 1)
                    rack_.requestUnpatch(inlet);
                else
                    rack_.requestPatch(inlet, (s42::Outlet) (id - 2));
            };
            addAndMakeVisible(*box);
            boxes_.add(box.release());
        }
    }

    int preferredHeight() const { return s42::kInletCount * kRowH + 8; }

    void resized() override
    {
        auto r = getLocalBounds().reduced(4);
        for (int i = 0; i < labels_.size(); ++i)
        {
            auto row = r.removeFromTop(kRowH);
            labels_[i]->setBounds(row.removeFromLeft(150));
            boxes_[i]->setBounds(row.reduced(2, 3));
        }
    }

private:
    static constexpr int kRowH = 26;
    s42::Rack& rack_;
    juce::OwnedArray<juce::Label> labels_;
    juce::OwnedArray<juce::ComboBox> boxes_;
};
