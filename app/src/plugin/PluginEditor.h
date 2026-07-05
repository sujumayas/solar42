#pragma once

#include "PluginProcessor.h"
#include "ui/PanelView.h"
#include "ui/PresetBar.h"
#include "ui/SettingsDrawer.h"

// M5/M6 editor: the full panel (jacks + cables + performance zone) scaled
// from the logical 4950 x 3200 space. Zoom 100-300 % (Cmd+scroll / pinch
// anchored at the cursor), scroll or drag empty panel space to pan,
// double-click a section title band to zoom to that section, double-click
// the background to fit. The keyboard's SETTINGS button opens the (desktop-
// space) settings drawer on the right edge.
//
// M7 adds the preset bar as a proportional strip ABOVE the panel (the editor
// aspect covers panel + bar), a shared TooltipWindow, and editor-size
// persistence through the UI state child.
class Solar42NEditor : public juce::AudioProcessorEditor
{
public:
    explicit Solar42NEditor(Solar42NProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;

private:
    static constexpr int kBarLogical = 140; // preset-bar height in panel units

    float baseScale() const noexcept;
    float scale() const noexcept { return baseScale() * zoom_; }
    int barHeight() const noexcept;
    float viewportHeight() const noexcept; // editor height minus the bar
    void applyTransform();
    void zoomAt(juce::Point<float> viewPos, float newZoom);
    void zoomToRect(juce::Rectangle<int> panelRect);

    Solar42NProcessor& proc_;
    solar::PresetBar bar_;
    solar::PanelView panel_;
    solar::SettingsDrawer drawer_;
    juce::TooltipWindow tooltips_ { this, 700 };
    float zoom_ = 1.0f;
    juce::Point<float> pan_; // logical top-left of the visible window

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NEditor)
};
