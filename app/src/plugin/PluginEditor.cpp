#include "PluginEditor.h"

namespace
{
constexpr double kAspect = (double) solar::PanelView::kLogicalW
                           / ((double) solar::PanelView::kLogicalH + 140.0);
} // namespace

Solar42NEditor::Solar42NEditor(Solar42NProcessor& p)
    : juce::AudioProcessorEditor(p),
      proc_(p),
      bar_(p.presets()),
      panel_(p.apvts(), p.rack(), p.patchBay(), p.keyboardState(), p.kbTouch()),
      drawer_(p.keyboardState())
{
    addAndMakeVisible(bar_);
    addAndMakeVisible(panel_);
    addChildComponent(drawer_); // hidden until the keyboard's SETTINGS button
    bar_.toFront(false);

    panel_.onPan = [this](juce::Point<float> screenDelta)
    {
        pan_ -= screenDelta / scale();
        applyTransform();
    };
    panel_.onZoomToRect = [this](juce::Rectangle<int> r) { zoomToRect(r); };
    panel_.onOpenKeyboardSettings = [this]
    {
        drawer_.setVisible(!drawer_.isVisible());
        drawer_.toFront(false);
    };
    drawer_.onClose = [this] { drawer_.setVisible(false); };

    setResizable(true, true);
    setResizeLimits(900, (int) std::lround(900.0 / kAspect),
                    3200, (int) std::lround(3200.0 / kAspect));
    getConstrainer()->setFixedAspectRatio(kAspect);

    // Editor size persists in the UI state child (M7 convenience).
    const auto ui = proc_.apvts().state.getChildWithName("UI");
    const int w = juce::jlimit(900, 3200, (int) ui.getProperty("editorW", 1280));
    setSize(w, (int) std::lround((double) w / kAspect));
}

void Solar42NEditor::paint(juce::Graphics& g)
{
    g.fillAll(solar::kCream.darker(0.35f));
}

float Solar42NEditor::baseScale() const noexcept
{
    return (float) getWidth() / (float) solar::PanelView::kLogicalW;
}

int Solar42NEditor::barHeight() const noexcept
{
    return (int) std::lround((double) getWidth() * kBarLogical
                             / (double) solar::PanelView::kLogicalW);
}

float Solar42NEditor::viewportHeight() const noexcept
{
    return (float) (getHeight() - barHeight());
}

void Solar42NEditor::applyTransform()
{
    const float s = scale();
    pan_.x = juce::jlimit(0.0f, juce::jmax(0.0f, (float) solar::PanelView::kLogicalW
                                                     - (float) getWidth() / s), pan_.x);
    pan_.y = juce::jlimit(0.0f, juce::jmax(0.0f, (float) solar::PanelView::kLogicalH
                                                     - viewportHeight() / s), pan_.y);
    panel_.setTransform(juce::AffineTransform::translation(-pan_.x, -pan_.y)
                            .scaled(s)
                            .translated(0.0f, (float) barHeight()));
    panel_.setTopLeftPosition(0, 0);
}

void Solar42NEditor::resized()
{
    bar_.setBounds(0, 0, getWidth(), barHeight());
    applyTransform();
    drawer_.setBounds(getLocalBounds()
                          .withTrimmedTop(barHeight())
                          .removeFromRight(juce::jmin(360, getWidth() / 3)));

    auto ui = proc_.apvts().state.getOrCreateChildWithName("UI", nullptr);
    ui.setProperty("editorW", getWidth(), nullptr);
}

void Solar42NEditor::zoomAt(juce::Point<float> viewPos, float newZoom)
{
    newZoom = juce::jlimit(1.0f, 3.0f, newZoom);
    const auto panelPos = viewPos - juce::Point<float>(0.0f, (float) barHeight());
    const auto logical = pan_ + panelPos / scale(); // keep this point under the cursor
    zoom_ = newZoom;
    pan_ = logical - panelPos / scale();
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
                                    viewportHeight() / (r.getHeight() * baseScale())));
    pan_ = { r.getCentreX() - (float) getWidth() / (2.0f * scale()),
             r.getCentreY() - viewportHeight() / (2.0f * scale()) };
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
