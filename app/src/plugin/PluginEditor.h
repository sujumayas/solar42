#pragma once

#include "PluginProcessor.h"

class Solar42NEditor : public juce::AudioProcessorEditor
{
public:
    explicit Solar42NEditor(Solar42NProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    // Hardware panel is 49.5 x 32 cm; all UI works in a 4950 x 3200 logical space.
    static constexpr double kPanelAspect = 4950.0 / 3200.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NEditor)
};
