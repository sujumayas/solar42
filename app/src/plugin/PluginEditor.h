#pragma once

#include "PluginProcessor.h"
#include "ui/PanelView.h"

// M5 editor: the full panel (jacks + cables + performance zone) scaled from
// the logical 4950 x 3200 space. Zoom 100-300 % (Cmd+scroll / pinch anchored
// at the cursor), scroll or drag empty panel space to pan, double-click a
// section title band to zoom to that section, double-click the background to
// fit. The M2 debug patch matrix is gone — the CableLayer is the patching UI.
class Solar42NEditor : public juce::AudioProcessorEditor
{
public:
    explicit Solar42NEditor(Solar42NProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;

private:
    float baseScale() const noexcept;
    float scale() const noexcept { return baseScale() * zoom_; }
    void applyTransform();
    void zoomAt(juce::Point<float> viewPos, float newZoom);
    void zoomToRect(juce::Rectangle<int> panelRect);

    solar::PanelView panel_;
    float zoom_ = 1.0f;
    juce::Point<float> pan_; // logical top-left of the visible window

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NEditor)
};
