#include "PluginEditor.h"

Solar42NEditor::Solar42NEditor(Solar42NProcessor& p)
    : juce::AudioProcessorEditor(p), panel_(p.apvts()), matrix_(p.rack())
{
    addAndMakeVisible(panel_);
    matrixView_.setViewedComponent(&matrix_, false);
    matrixView_.setScrollBarsShown(true, false);
    addAndMakeVisible(matrixView_);

    setResizable(true, true);
    setResizeLimits(900, 620, 2400, 1700);
    setSize(1280, 840);
}

void Solar42NEditor::paint(juce::Graphics& g)
{
    g.fillAll(solar::kCream.darker(0.06f));
}

void Solar42NEditor::resized()
{
    auto r = getLocalBounds();

    // Panel: fixed logical aspect, scaled to the editor width.
    const float scale = (float) r.getWidth() / (float) solar::PanelView::kLogicalW;
    const int panelH = (int) ((float) solar::PanelView::kLogicalH * scale);
    panel_.setTransform(juce::AffineTransform::scale(scale));
    panel_.setTopLeftPosition(0, 0);
    r.removeFromTop(panelH);

    // Performance zone: the debug patch matrix until M5's cable layer.
    matrixView_.setBounds(r.reduced(8));
    matrix_.setSize(matrixView_.getWidth() - matrixView_.getScrollBarThickness() - 2,
                    matrix_.preferredHeight());
}
