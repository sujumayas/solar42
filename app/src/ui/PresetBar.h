#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "state/PresetManager.h"
#include "ui/SolarLookAndFeel.h"

namespace solar {

// The preset strip above the panel — deliberately OUTSIDE the skeuomorphic
// surface (plan §state): prev/next steppers, the current preset name opening
// the full menu (factory + user + utilities), and a SAVE button. Height is
// proportional to the editor width, so it scales with the fixed aspect.
class PresetBar : public juce::Component,
                  private juce::ChangeListener
{
public:
    explicit PresetBar(PresetManager& pm) : pm_(pm)
    {
        prev_.setButtonText("<");
        next_.setButtonText(">");
        save_.setButtonText("SAVE");
        name_.setButtonText(pm_.currentName());
        prev_.setTooltip("Previous preset (factory list, then your saved presets)");
        next_.setTooltip("Next preset");
        save_.setTooltip("Save the whole rig - knobs, cables, keyboard setup, "
                         "cartridge slots - as a user preset (.s42n)");
        name_.setTooltip("Preset menu: factory presets, user presets, utilities");

        prev_.onClick = [this] { pm_.loadNext(-1); };
        next_.onClick = [this] { pm_.loadNext(1); };
        name_.onClick = [this] { showMenu(); };
        save_.onClick = [this] { promptSave(); };

        for (auto* b : { &prev_, &name_, &next_, &save_ })
            addAndMakeVisible(b);
        pm_.addChangeListener(this);
    }

    ~PresetBar() override { pm_.removeChangeListener(this); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kInk);
        g.setColour(kCream.withAlpha(0.85f));
        g.setFont(juce::FontOptions((float) getHeight() * 0.42f, juce::Font::bold));
        g.drawText("SOLAR 42N", getLocalBounds().removeFromLeft(getWidth() / 8).reduced(12, 0),
                   juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(juce::jmax(2, getHeight() / 8));
        const int h = r.getHeight();
        r.removeFromLeft(getWidth() / 8); // wordmark
        prev_.setBounds(r.removeFromLeft(h * 3 / 2));
        r.removeFromLeft(4);
        name_.setBounds(r.removeFromLeft(getWidth() / 4));
        r.removeFromLeft(4);
        next_.setBounds(r.removeFromLeft(h * 3 / 2));
        r.removeFromLeft(12);
        save_.setBounds(r.removeFromLeft(h * 3));
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        name_.setButtonText(pm_.currentName());
    }

    void showMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader("FACTORY");
        for (int i = 0; i < pm_.numFactory(); ++i)
            m.addItem(1 + i, pm_.factoryName(i) + "  -  " + pm_.factoryHint(i), true,
                      pm_.currentName() == pm_.factoryName(i));

        const auto users = pm_.userPresetNames();
        if (!users.isEmpty())
        {
            m.addSectionHeader("USER");
            for (int i = 0; i < users.size(); ++i)
                m.addItem(1000 + i, users[i], true, pm_.currentName() == users[i]);
        }

        m.addSeparator();
        m.addItem(-1, "Save preset...");
        m.addItem(-2, "Open presets folder");
        m.addSeparator();
        m.addItem(-3, "Swap unit (new component-tolerance serial)");

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&name_),
                        [this, users](int id)
                        {
                            if (id == 0)
                                return;
                            if (id >= 1000)
                                pm_.loadUser(users[id - 1000]);
                            else if (id >= 1)
                                pm_.loadFactory(id - 1);
                            else if (id == -1)
                                promptSave();
                            else if (id == -2)
                                pm_.userDir().revealToUser();
                            else if (id == -3)
                                pm_.swapUnitSerial();
                        });
    }

    void promptSave()
    {
        auto* w = new juce::AlertWindow("Save preset",
                                        "Saves the complete rig state as a .s42n file.",
                                        juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", pm_.currentName(), "Preset name:");
        w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true,
                           juce::ModalCallbackFunction::create(
                               [this, w](int result)
                               {
                                   const auto name = w->getTextEditorContents("name").trim();
                                   if (result == 1 && name.isNotEmpty())
                                       pm_.saveUser(name);
                               }),
                           true); // delete the window when dismissed
    }

    PresetManager& pm_;
    juce::TextButton prev_, next_, name_, save_;
};

} // namespace solar
