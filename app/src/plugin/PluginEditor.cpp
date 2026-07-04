#include "PluginEditor.h"

namespace
{
// Panel cream sampled from reference-docs/solar42n-panel-1.png.
const juce::Colour kPanelCream { 0xffe9e4d8 };
const juce::Colour kInk { 0xff17171a };
const juce::Colour kAccentRed { 0xffc8271e };
constexpr int kHeader = 44;
} // namespace

Solar42NEditor::Solar42NEditor(Solar42NProcessor& p)
    : juce::AudioProcessorEditor(p), params_(p)
{
    addAndMakeVisible(params_);
    setResizable(true, true);
    setResizeLimits(420, 480, 900, 1400);
    setSize(560, 820);
}

void Solar42NEditor::paint(juce::Graphics& g)
{
    g.fillAll(kPanelCream);

    auto header = getLocalBounds().removeFromTop(kHeader).toFloat().reduced(12.0f, 6.0f);
    g.setColour(kInk);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("SOLAR 42", header, juce::Justification::centredLeft);

    g.setColour(kAccentRed);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("M1 debug rig — DRONE 1 · gate = keypad button, hold = latch",
               header, juce::Justification::centredRight);
}

void Solar42NEditor::resized()
{
    params_.setBounds(getLocalBounds().withTrimmedTop(kHeader));
}
