#pragma once

#include "PluginProcessor.h"

// M1 debug editor: branded strip + auto-generated parameter list. The real
// skeuomorphic PanelEditor replaces this from M3 onward.
class Solar42NEditor : public juce::AudioProcessorEditor
{
public:
    explicit Solar42NEditor(Solar42NProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor params_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NEditor)
};
