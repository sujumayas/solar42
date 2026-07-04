#pragma once

#include "PluginProcessor.h"
#include "ui/PatchMatrixDebug.h"

// M2 debug editor: branded strip + auto-generated parameter list (left) +
// patch matrix (right). The real skeuomorphic PanelEditor replaces this from
// M3/M5 onward.
class Solar42NEditor : public juce::AudioProcessorEditor
{
public:
    explicit Solar42NEditor(Solar42NProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor params_;
    PatchMatrixDebug matrix_;
    juce::Viewport matrixView_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NEditor)
};
