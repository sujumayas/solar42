#pragma once

#include "PluginProcessor.h"
#include "ui/PanelView.h"
#include "ui/PatchMatrixDebug.h"

// M3 editor: panel UI phase 1 (top half + mod strip, scaled from the logical
// 4950-wide space) with the M2 debug patch matrix in the performance zone.
// M5 replaces the matrix with the CableLayer and adds the print/jack layer.
class Solar42NEditor : public juce::AudioProcessorEditor
{
public:
    explicit Solar42NEditor(Solar42NProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    solar::PanelView panel_;
    PatchMatrixDebug matrix_;
    juce::Viewport matrixView_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NEditor)
};
