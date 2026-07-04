#include "PluginEditor.h"

namespace
{
// Panel cream sampled from reference-docs/solar42n-panel-1.png.
const juce::Colour kPanelCream { 0xffe9e4d8 };
const juce::Colour kInk { 0xff17171a };
const juce::Colour kAccentRed { 0xffc8271e };
} // namespace

Solar42NEditor::Solar42NEditor(Solar42NProcessor& p)
    : juce::AudioProcessorEditor(p)
{
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(kPanelAspect);
    setResizeLimits(742, 480, 2475, 1600);
    setSize(1113, 720);
}

void Solar42NEditor::paint(juce::Graphics& g)
{
    g.fillAll(kPanelCream);

    auto r = getLocalBounds().toFloat().reduced(getWidth() * 0.04f);
    g.setColour(kInk);
    g.setFont(juce::FontOptions((float) getWidth() * 0.055f, juce::Font::bold));
    g.drawText("SOLAR 42", r.removeFromTop(r.getHeight() * 0.25f),
               juce::Justification::topLeft);

    g.setColour(kAccentRed);
    g.setFont(juce::FontOptions((float) getWidth() * 0.018f));
    g.drawText("digital instrument — M0 skeleton (panel arrives in M3–M5)",
               getLocalBounds().toFloat().reduced(getWidth() * 0.04f),
               juce::Justification::bottomLeft);
}
