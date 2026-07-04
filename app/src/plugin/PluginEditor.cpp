#include "PluginEditor.h"

Solar42NEditor::Solar42NEditor(Solar42NProcessor& p)
    : juce::AudioProcessorEditor(p), panel_(p.apvts(), p.rack(), p.patchBay())
{
    addAndMakeVisible(panel_);

    panel_.onPan = [this](juce::Point<float> screenDelta)
    {
        pan_ -= screenDelta / scale();
        applyTransform();
    };
    panel_.onZoomToRect = [this](juce::Rectangle<int> r) { zoomToRect(r); };

    setResizable(true, true);
    setResizeLimits(900, 580, 3200, 2070);
    getConstrainer()->setFixedAspectRatio((double) solar::PanelView::kLogicalW
                                          / (double) solar::PanelView::kLogicalH);
    setSize(1280, 828);
}

void Solar42NEditor::paint(juce::Graphics& g)
{
    g.fillAll(solar::kCream.darker(0.35f));
}

float Solar42NEditor::baseScale() const noexcept
{
    return (float) getWidth() / (float) solar::PanelView::kLogicalW;
}

void Solar42NEditor::applyTransform()
{
    const float s = scale();
    pan_.x = juce::jlimit(0.0f, juce::jmax(0.0f, (float) solar::PanelView::kLogicalW
                                                     - (float) getWidth() / s), pan_.x);
    pan_.y = juce::jlimit(0.0f, juce::jmax(0.0f, (float) solar::PanelView::kLogicalH
                                                     - (float) getHeight() / s), pan_.y);
    panel_.setTransform(juce::AffineTransform::translation(-pan_.x, -pan_.y).scaled(s));
    panel_.setTopLeftPosition(0, 0);
}

void Solar42NEditor::resized()
{
    applyTransform();
}

void Solar42NEditor::zoomAt(juce::Point<float> viewPos, float newZoom)
{
    newZoom = juce::jlimit(1.0f, 3.0f, newZoom);
    const auto logical = pan_ + viewPos / scale(); // keep this point under the cursor
    zoom_ = newZoom;
    pan_ = logical - viewPos / scale();
    applyTransform();
}

void Solar42NEditor::zoomToRect(juce::Rectangle<int> panelRect)
{
    if (panelRect.getWidth() >= solar::PanelView::kLogicalW || zoom_ > 2.6f)
    {
        zoom_ = 1.0f; // fit / toggle back out
        pan_ = {};
        applyTransform();
        return;
    }
    const auto r = panelRect.toFloat().expanded(60.0f);
    zoom_ = juce::jlimit(1.0f, 3.0f,
                         juce::jmin((float) getWidth() / (r.getWidth() * baseScale()),
                                    (float) getHeight() / (r.getHeight() * baseScale())));
    pan_ = { r.getCentreX() - (float) getWidth() / (2.0f * scale()),
             r.getCentreY() - (float) getHeight() / (2.0f * scale()) };
    applyTransform();
}

void Solar42NEditor::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel)
{
    const auto viewPos = e.getEventRelativeTo(this).position;
    if (e.mods.isCommandDown())
    {
        zoomAt(viewPos, zoom_ * (1.0f + wheel.deltaY * 1.6f));
        return;
    }
    pan_ += juce::Point<float>(-wheel.deltaX, -wheel.deltaY) * 1400.0f / zoom_;
    applyTransform();
}

void Solar42NEditor::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    zoomAt(e.getEventRelativeTo(this).position, zoom_ * scaleFactor);
}
