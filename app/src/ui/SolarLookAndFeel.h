#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Panel theme + widget drawing for the Solar 42N (panel UI phase 1).
// Colors follow the hardware code (07 §0): knob caps black/blue/green/grey/
// orange/red by function; panel cream; ink print. Positions are M5's job —
// this file is only "how things look".
namespace solar {

// Sampled from reference-docs/solar42n-panel-1.png.
inline const juce::Colour kCream { 0xffe9e4d8 };
inline const juce::Colour kInk { 0xff17171a };
inline const juce::Colour kAccentRed { 0xffc8271e };
inline const juce::Colour kSectionBg { 0xffefeadf };  // slightly lighter than field
inline const juce::Colour kHeaderBand { 0xff17171a }; // section title bands

// Hardware knob color code.
inline const juce::Colour kKnobBlack { 0xff222226 };
inline const juce::Colour kKnobBlue { 0xff2e7d9c };   // VOLT + Papa Srapa
inline const juce::Colour kKnobGreen { 0xff2e7d4f };  // VCO + envelopes
inline const juce::Colour kKnobGrey { 0xffb9b5ab };   // mixer VOL
inline const juce::Colour kKnobOrange { 0xffe08a2e }; // filter/effector/master
inline const juce::Colour kKnobRed { 0xffc8271e };    // mod strip

class SolarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SolarLookAndFeel()
    {
        setColour(juce::Label::textColourId, kInk);
        setColour(juce::Slider::textBoxTextColourId, kInk);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId, kCream);
        setColour(juce::ComboBox::textColourId, kInk);
        setColour(juce::ComboBox::outlineColourId, kInk);
        setColour(juce::ComboBox::arrowColourId, kInk);
        setColour(juce::PopupMenu::backgroundColourId, kCream);
        setColour(juce::PopupMenu::textColourId, kInk);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccentRed);
        setColour(juce::TooltipWindow::backgroundColourId, kInk);
    }

    // Knob cap colour rides on the slider's rotarySliderFillColourId.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float radius = size * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Tick ring (panel print).
        g.setColour(kInk.withAlpha(0.55f));
        for (int t = 0; t <= 10; ++t)
        {
            const float a = rotaryStartAngle + (float) t / 10.0f * (rotaryEndAngle - rotaryStartAngle);
            const auto p1 = centre.getPointOnCircumference(radius * 0.98f, a);
            const auto p2 = centre.getPointOnCircumference(radius * 0.88f, a);
            g.drawLine({ p1, p2 }, size * 0.015f + 0.6f);
        }

        // Black skirt + colored cap.
        const float skirtR = radius * 0.82f;
        g.setColour(kKnobBlack);
        g.fillEllipse(centre.x - skirtR, centre.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);

        const auto cap = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const float capR = radius * 0.62f;
        g.setColour(cap);
        g.fillEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
        g.setColour(kInk.withAlpha(0.35f));
        g.drawEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        // Pointer.
        const auto tip = centre.getPointOnCircumference(skirtR * 0.96f, angle);
        const auto tail = centre.getPointOnCircumference(capR * 0.15f, angle);
        g.setColour(kCream);
        g.drawLine({ tail, tip }, juce::jmax(2.0f, size * 0.055f));
    }

    // Two styles of button, selected via component properties:
    //   props["switch"] = 1  -> vertical slide switch (fm/am, BP/LP, x10...)
    //   otherwise            -> round push button with an LED dot (MUTE/MOD/HOLD)
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                          bool highlighted, bool /*down*/) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        if ((int) b.getProperties().getWithDefault("switch", 0) != 0)
        {
            // Slide switch: label to the right of a small vertical track.
            const float trackW = juce::jmin(r.getHeight() * 0.55f, 14.0f);
            auto track = r.removeFromLeft(trackW).reduced(1.0f);
            g.setColour(kInk);
            g.fillRoundedRectangle(track, trackW * 0.3f);
            auto thumb = track.reduced(2.0f);
            thumb = on ? thumb.removeFromTop(thumb.getHeight() * 0.45f)
                       : thumb.removeFromBottom(thumb.getHeight() * 0.45f);
            g.setColour(highlighted ? kCream : kCream.darker(0.1f));
            g.fillRoundedRectangle(thumb, trackW * 0.25f);

            g.setColour(kInk);
            g.setFont(juce::FontOptions(juce::jmin(12.0f, r.getHeight() * 0.5f)));
            g.drawText(b.getButtonText(), r.withTrimmedLeft(3), juce::Justification::centredLeft);
            return;
        }

        // Push button with LED. LED colour rides on the button's on-colour.
        const float d = juce::jmin(r.getWidth(), r.getHeight() * 0.78f);
        auto circle = juce::Rectangle<float>(d, d)
                          .withCentre({ r.getCentreX(), r.getY() + d * 0.5f + 1.0f });
        g.setColour(kKnobBlack);
        g.fillEllipse(circle);
        g.setColour(highlighted ? kCream : kInk.brighter(0.25f));
        g.drawEllipse(circle.reduced(0.5f), 1.2f);

        const auto led = b.findColour(juce::TextButton::buttonOnColourId);
        g.setColour(on ? led : led.withAlpha(0.15f));
        const float ledR = d * 0.16f;
        g.fillEllipse(circle.getCentreX() - ledR, circle.getCentreY() - ledR,
                      ledR * 2.0f, ledR * 2.0f);

        g.setColour(kInk);
        g.setFont(juce::FontOptions(juce::jmin(11.0f, r.getHeight() * 0.3f)));
        g.drawText(b.getButtonText(), r.withTop(circle.getBottom()),
                   juce::Justification::centredTop);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return juce::Font(juce::FontOptions(juce::jmin(13.0f, (float) box.getHeight() * 0.6f)));
    }
};

} // namespace solar
