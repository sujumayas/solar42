#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/SolarLookAndFeel.h"
#include "ui/Tooltips.h"

// One component per silk-screened section, laid out in LOGICAL panel units as
// fractions of each section's rectangle — the same fractions the jack census
// in PanelLayout.h uses, so widgets and the (globally drawn) jacks interleave
// exactly like the render. The host editor scales the whole panel with one
// AffineTransform; M5's zoom rides on that.
//
// Empty section space drags the panel (pan) and a double-click on a title
// band zooms to the section — sections forward both to the PanelSurface.
namespace solar {

using Apvts = juce::AudioProcessorValueTreeState;

// Implemented by PanelView; sections find it through the parent chain.
struct PanelSurface
{
    virtual ~PanelSurface() = default;
    virtual void panByScreenDelta(juce::Point<float> d) = 0;
    virtual void zoomToPanelRect(juce::Rectangle<int> r) = 0;
};

// ---------------------------------------------------------------- widget kit

struct LabeledKnob : juce::Component
{
    LabeledKnob(Apvts& s, const juce::String& paramId, const juce::String& text,
                juce::Colour cap)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, cap);
        addAndMakeVisible(slider);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setMinimumHorizontalScale(0.7f);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
        attach = std::make_unique<Apvts::SliderAttachment>(s, paramId, slider);

        // M7 conveniences: hint, value bubble in real units (the attachment
        // wires the parameter's text functions), double-click = default.
        if (auto* p = s.getParameter(paramId))
        {
            slider.setTooltip(tips::controlTooltip(paramId, p->getName(64)));
            slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
            slider.setPopupDisplayEnabled(true, false, nullptr);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto text = r.removeFromBottom(juce::jmax(30, r.getHeight() / 5));
        // Legibility floor (M9b): never render print below ~hardware size.
        label.setFont(juce::FontOptions(
            juce::jlimit(26.0f, 40.0f, (float) text.getHeight() * 0.75f)));
        label.setBounds(text);
        slider.setBounds(r);
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<Apvts::SliderAttachment> attach;
};

// Vertical slide switch (fm/am, BP/LP, x1/x10, lin/exp, oct, sub, link...).
struct SlideSwitch : juce::Component
{
    SlideSwitch(Apvts& s, const juce::String& paramId, const juce::String& text)
    {
        button.setButtonText(text);
        button.getProperties().set("switch", 1);
        addAndMakeVisible(button);
        attach = std::make_unique<Apvts::ButtonAttachment>(s, paramId, button);
        if (auto* p = s.getParameter(paramId))
            button.setTooltip(tips::controlTooltip(paramId, p->getName(64)));
    }

    void resized() override { button.setBounds(getLocalBounds()); }

    juce::ToggleButton button;
    std::unique_ptr<Apvts::ButtonAttachment> attach;
};

// Round push button with an LED dot (MUTE / MOD / HOLD / keypad).
struct PushButton : juce::Component
{
    PushButton(Apvts& s, const juce::String& paramId, const juce::String& text,
               juce::Colour led)
    {
        button.setButtonText(text);
        button.setColour(juce::TextButton::buttonOnColourId, led);
        addAndMakeVisible(button);
        attach = std::make_unique<Apvts::ButtonAttachment>(s, paramId, button);
        if (auto* p = s.getParameter(paramId))
            button.setTooltip(tips::controlTooltip(paramId, p->getName(64)));
    }

    void resized() override { button.setBounds(getLocalBounds()); }

    juce::ToggleButton button;
    std::unique_ptr<Apvts::ButtonAttachment> attach;
};

// Labeled combo for the few stepped controls (DIVIDER, LFO range, STAGES).
struct ChoiceBox : juce::Component
{
    ChoiceBox(Apvts& s, const juce::String& paramId, const juce::String& text)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(s.getParameter(paramId)))
        {
            box.addItemList(p->choices, 1);
            box.setTooltip(tips::controlTooltip(paramId, p->getName(64)));
        }
        addAndMakeVisible(box);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
        attach = std::make_unique<Apvts::ComboBoxAttachment>(s, paramId, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (label.getText().isNotEmpty())
        {
            auto text = r.removeFromBottom(juce::jmax(30, r.getHeight() / 3));
            label.setFont(juce::FontOptions(
                juce::jlimit(26.0f, 36.0f, (float) text.getHeight() * 0.7f)));
            label.setBounds(text);
        }
        box.setBounds(r.reduced(2, juce::jmax(0, (r.getHeight() - 92) / 2)));
    }

    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<Apvts::ComboBoxAttachment> attach;
};

// Vertical multi-position slide switch bound to an AudioParameterChoice —
// the hardware control behind what M5 stubbed as combo boxes (LFO x1/x6/x10
// range, sequencer STAGES). Position labels print beside the track in the
// hardware's top-to-bottom order, which need not match choice-index order
// (e.g. the LFO range reads x6 / x1 / x10 with x1 in the centre detent).
struct ChoiceSlideSwitch : juce::Component,
                           public juce::SettableTooltipClient
{
    ChoiceSlideSwitch(Apvts& s, const juce::String& paramId, juce::StringArray posLabels,
                      std::vector<int> posToChoice, juce::String caption = {})
        : labels_(std::move(posLabels)), map_(std::move(posToChoice)),
          caption_(std::move(caption)),
          att_(*s.getParameter(paramId),
               [this](float v)
               {
                   choice_ = (int) std::lround(v);
                   repaint();
               })
    {
        if (auto* p = s.getParameter(paramId))
            setTooltip(tips::controlTooltip(paramId, p->getName(64)));
        att_.sendInitialUpdate();
    }

    void mouseDown(const juce::MouseEvent& e) override { pick(e); }
    void mouseDrag(const juce::MouseEvent& e) override { pick(e); }

    void paint(juce::Graphics& g) override
    {
        const int n = labels_.size();
        auto r = getLocalBounds().toFloat();
        if (caption_.isNotEmpty())
        {
            auto cap = r.removeFromBottom(juce::jmax(30.0f, r.getHeight() * 0.18f));
            g.setColour(kInk);
            g.setFont(juce::FontOptions(juce::jlimit(24.0f, 30.0f, cap.getHeight() * 0.8f),
                                        juce::Font::bold));
            g.drawText(caption_, cap, juce::Justification::centred);
        }

        const auto track = trackRect(r);
        g.setColour(kInk);
        g.fillRoundedRectangle(track, track.getWidth() * 0.4f);

        int pos = 0; // active detent (first position wins if unmapped)
        for (int i = 0; i < n; ++i)
            if (map_[(size_t) i] == choice_)
                pos = i;

        g.setFont(juce::FontOptions(juce::jmin(30.0f, r.getHeight() / (float) n * 0.62f),
                                    juce::Font::bold));
        for (int i = 0; i < n; ++i)
        {
            g.setColour(kInk.withAlpha(i == pos ? 1.0f : 0.55f));
            g.drawText(labels_[i],
                       juce::Rectangle<float>(track.getRight() + 10.0f, slotY(r, i) - 18.0f,
                                              r.getRight() - track.getRight() - 12.0f, 36.0f),
                       juce::Justification::centredLeft);
        }

        const float hw = track.getWidth() * 1.7f;
        g.setColour(juce::Colour(0xffe4e0d4));
        g.fillRoundedRectangle(track.getCentreX() - hw * 0.5f, slotY(r, pos) - hw * 0.32f,
                               hw, hw * 0.64f, 6.0f);
        g.setColour(kInk);
        g.drawRoundedRectangle(track.getCentreX() - hw * 0.5f, slotY(r, pos) - hw * 0.32f,
                               hw, hw * 0.64f, 6.0f, 2.5f);
    }

private:
    juce::Rectangle<float> trackRect(juce::Rectangle<float> r) const
    {
        const float w = juce::jlimit(26.0f, 40.0f, r.getWidth() * 0.18f);
        return { r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.08f,
                 w, r.getHeight() * 0.84f };
    }

    float slotY(juce::Rectangle<float> r, int i) const
    {
        const auto t = trackRect(r);
        const int n = juce::jmax(2, labels_.size());
        return t.getY() + t.getHeight() * (0.12f + 0.76f * (float) i / (float) (n - 1));
    }

    void pick(const juce::MouseEvent& e)
    {
        auto r = getLocalBounds().toFloat();
        if (caption_.isNotEmpty())
            r.removeFromBottom(juce::jmax(30.0f, r.getHeight() * 0.18f));
        int best = 0;
        float bestD = 1.0e9f;
        for (int i = 0; i < labels_.size(); ++i)
        {
            const float d = std::abs((float) e.y - slotY(r, i));
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        if (map_[(size_t) best] != choice_)
            att_.setValueAsCompleteGesture((float) map_[(size_t) best]);
    }

    juce::StringArray labels_;
    std::vector<int> map_;
    juce::String caption_;
    int choice_ = 0;
    juce::ParameterAttachment att_;
};

// The effector's 1-2-3 program toggle: a real 3-position slide switch (M7,
// replacing the M5 combo-box placeholder). Click a detent or drag; flipping
// it is the hardware act that loads a program from the inserted cartridge.
struct Prog3Switch : juce::Component,
                     public juce::SettableTooltipClient
{
    Prog3Switch(Apvts& s, const juce::String& paramId, const juce::String& text)
        : title_(text),
          att_(*s.getParameter(paramId),
               [this](float v)
               {
                   idx_ = juce::jlimit(0, 2, (int) std::lround(v));
                   repaint();
               })
    {
        if (auto* p = s.getParameter(paramId))
            setTooltip(tips::controlTooltip(paramId, p->getName(64)));
        att_.sendInitialUpdate();
    }

    void mouseDown(const juce::MouseEvent& e) override { pick(e); }
    void mouseDrag(const juce::MouseEvent& e) override { pick(e); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto head = r.removeFromTop(r.getHeight() * 0.22f);
        g.setColour(kInk);
        g.setFont(juce::FontOptions(head.getHeight() * 0.9f, juce::Font::bold));
        g.drawText(title_, head, juce::Justification::centred);

        const auto track = trackRect();
        g.setColour(juce::Colour(0xff17171a));
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);

        g.setFont(juce::FontOptions(juce::jmax(28.0f, track.getWidth() * 0.62f),
                                    juce::Font::bold));
        for (int i = 0; i < 3; ++i)
        {
            g.setColour(kInk.withAlpha(i == idx_ ? 1.0f : 0.45f));
            g.drawText(juce::String(i + 1),
                       juce::Rectangle<float>(juce::jmax(0.0f, track.getX() - 54.0f),
                                              slotY(i) - 18.0f, 40.0f, 36.0f),
                       juce::Justification::centredRight);
        }

        // Handle at the selected detent (narrow enough to spare the digits).
        const float hw = track.getWidth() * 1.6f;
        g.setColour(juce::Colour(0xffe4e0d4));
        g.fillRoundedRectangle(track.getCentreX() - hw * 0.5f, slotY(idx_) - hw * 0.35f,
                               hw, hw * 0.7f, 6.0f);
        g.setColour(kInk);
        g.drawRoundedRectangle(track.getCentreX() - hw * 0.5f, slotY(idx_) - hw * 0.35f,
                               hw, hw * 0.7f, 6.0f, 2.5f);
    }

private:
    juce::Rectangle<float> trackRect() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromTop(r.getHeight() * 0.22f);
        const float w = juce::jlimit(26.0f, 40.0f, r.getWidth() * 0.24f);
        // Off-centre right so the 1/2/3 digits fit inside the component.
        return { r.getCentreX() + 4.0f, r.getY() + r.getHeight() * 0.10f,
                 w, r.getHeight() * 0.80f };
    }

    float slotY(int i) const
    {
        const auto t = trackRect();
        return t.getY() + t.getHeight() * (0.14f + 0.36f * (float) i);
    }

    void pick(const juce::MouseEvent& e)
    {
        int best = 0;
        float bestD = 1.0e9f;
        for (int i = 0; i < 3; ++i)
        {
            const float d = std::abs((float) e.y - slotY(i));
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        if (best != idx_)
            att_.setValueAsCompleteGesture((float) best);
    }

    juce::String title_;
    int idx_ = 0;
    juce::ParameterAttachment att_;
};

// ---------------------------------------------------------------- section base

class Section : public juce::Component
{
public:
    enum class Band { Top, Bottom, None };

    explicit Section(juce::String title, Band band = Band::Top, juce::String badge = {})
        : title_(std::move(title)), badge_(std::move(badge)), band_(band)
    {
    }

    static constexpr int kBand = 64; // logical title-band height

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(kSectionBg);
        g.fillRoundedRectangle(r, 18.0f);
        g.setColour(kInk);
        g.drawRoundedRectangle(r, 18.0f, 5.0f);

        if (band_ != Band::None)
        {
            auto band = band_ == Band::Top ? r.removeFromTop((float) kBand)
                                           : r.removeFromBottom((float) kBand);
            g.fillRoundedRectangle(band.reduced(5.0f), 14.0f);
            g.setColour(kCream);
            g.setFont(juce::FontOptions((float) kBand * 0.62f, juce::Font::bold));
            g.drawText(title_, band.reduced(24.0f, 0.0f),
                       band_ == Band::Top ? juce::Justification::centredLeft
                                          : juce::Justification::centred);
        }

        if (badge_.isNotEmpty())
        {
            const auto b = frac(badgeFx_, badgeFy_, 0.0, 0.0).toFloat().withSizeKeepingCentre(58.0f, 58.0f);
            g.setColour(kInk);
            g.fillRoundedRectangle(b, 10.0f);
            g.setColour(kCream);
            g.setFont(juce::FontOptions(44.0f, juce::Font::bold));
            g.drawText(badge_, b, juce::Justification::centred);
        }

        paintExtras(g);
    }

    // Empty section space = pan; double-click the title band = zoom to section.
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (auto* s = findParentComponentOfClass<PanelSurface>())
            s->panByScreenDelta(e.getScreenPosition().toFloat() - lastScreenPos_);
        lastScreenPos_ = e.getScreenPosition().toFloat();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        lastScreenPos_ = e.getScreenPosition().toFloat();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const bool inBand = band_ == Band::None ? true
            : band_ == Band::Top ? e.y < kBand + 8
                                 : e.y > getHeight() - kBand - 8;
        if (inBand)
            if (auto* s = findParentComponentOfClass<PanelSurface>())
                s->zoomToPanelRect(getBounds());
    }

protected:
    virtual void paintExtras(juce::Graphics&) {}

    // Place by fractions of the SECTION rect — the same coordinate system the
    // jack census uses, so reserved jack spots line up by construction.
    juce::Rectangle<int> frac(double fx, double fy, double fw, double fh) const
    {
        return { (int) (fx * getWidth()), (int) (fy * getHeight()),
                 (int) (fw * getWidth()), (int) (fh * getHeight()) };
    }

    void place(juce::Component& c, double fx, double fy, double fw, double fh)
    {
        c.setBounds(frac(fx, fy, fw, fh));
    }

    static void led(juce::Graphics& g, juce::Point<float> centre, float level,
                    juce::Colour on, float r = 16.0f)
    {
        g.setColour(juce::Colour(0xff101013));
        g.fillEllipse(centre.x - r - 4.0f, centre.y - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f);
        g.setColour(on.withAlpha(0.12f + 0.88f * juce::jlimit(0.0f, 1.0f, level)));
        g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }

    juce::String title_, badge_;
    Band band_;
    double badgeFx_ = 0.955, badgeFy_ = 0.885; // sections with a busy corner override

private:
    juce::Point<float> lastScreenPos_;
};

// ---------------------------------------------------------------- voice sections

// Classic drone voice (DRONE 1/2/4/5): 5 gen columns MUTE/TUNE/MOD (hardware
// top-to-bottom order), red 5-LED bar, VOLT, photo-sensor window; bottom row
// HOLD/ATT/RLS between the jacks.
class ClassicDroneSection : public Section
{
public:
    ClassicDroneSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title, Band::Top, id.substring(1))
    {
        for (int g = 0; g < 5; ++g)
        {
            const auto base = id + ".gen" + juce::String(g + 1);
            mod[g] = std::make_unique<PushButton>(s, base + ".mod", "", kAccentRed);
            tune[g] = std::make_unique<LabeledKnob>(s, base + ".tune", "", kKnobBlack);
            mute[g] = std::make_unique<PushButton>(s, base + ".mute", "", kKnobGrey);
            addAndMakeVisible(*mod[g]);
            addAndMakeVisible(*tune[g]);
            addAndMakeVisible(*mute[g]);
        }
        volt = std::make_unique<LabeledKnob>(s, id + ".volt", "VOLT", kKnobBlue);
        hold = std::make_unique<PushButton>(s, id + ".hold", "HOLD", juce::Colour(0xffe8c33a));
        att = std::make_unique<LabeledKnob>(s, id + ".att", "ATT", kKnobBlack);
        rls = std::make_unique<LabeledKnob>(s, id + ".rls", "RLS", kKnobBlack);
        for (juce::Component* c : { (juce::Component*) volt.get(), (juce::Component*) hold.get(),
                                    (juce::Component*) att.get(), (juce::Component*) rls.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        // Hardware row order (render spec + manual print): numbers 1-5 on top,
        // then MUTE buttons / TUNE knobs / MOD buttons above the GATE block.
        for (int g = 0; g < 5; ++g)
        {
            const double x = 0.095 + 0.123 * g;
            place(*mute[g], x, 0.19, 0.115, 0.11);
            place(*tune[g], x, 0.31, 0.115, 0.30);
            place(*mod[g], x, 0.635, 0.115, 0.115);
        }
        place(*volt, 0.755, 0.28, 0.19, 0.34);
        place(*hold, 0.125, 0.775, 0.09, 0.155);
        place(*att, 0.225, 0.74, 0.135, 0.21);
        place(*rls, 0.365, 0.74, 0.135, 0.21);

        ledBar_ = frac(0.73, 0.13, 0.24, 0.10);
        sensor_ = frac(0.72, 0.70, 0.17, 0.22);
    }

    void setTelemetry(const float gens[5], float sensorGlow)
    {
        bool ledDiff = false;
        for (int g = 0; g < 5; ++g)
        {
            ledDiff = ledDiff || std::abs(gens[g] - gens_[g]) > 0.03f;
            gens_[g] = gens[g];
        }
        if (ledDiff)
            repaint(ledBar_.expanded(8));
        if (std::abs(sensorGlow - sensor01_) > 0.02f)
        {
            sensor01_ = sensorGlow;
            repaint(sensor_.expanded(8));
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Gen activity LED bar, "1 2 3 4 5" print beneath.
        auto bar = ledBar_.toFloat();
        g.setColour(juce::Colour(0xff101013));
        g.fillRoundedRectangle(bar, 8.0f);
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        for (int i = 0; i < 5; ++i)
        {
            const float cx = bar.getX() + bar.getWidth() * ((float) i + 0.5f) / 5.0f;
            led(g, { cx, bar.getCentreY() }, gens_[i], kAccentRed);
            g.setColour(kInk);
            g.drawText(juce::String(i + 1),
                       juce::Rectangle<float>(40.0f, 30.0f).withCentre({ cx, bar.getBottom() + 22.0f }),
                       juce::Justification::centred);
        }

        // Photo-sensor window glows with the LDR's light.
        const float d = juce::jmin((float) sensor_.getWidth(), (float) sensor_.getHeight());
        auto w = juce::Rectangle<float>(d, d).withCentre(sensor_.getCentre().toFloat());
        g.setColour(juce::Colours::white.interpolatedWith(juce::Colour(0xffff5040),
                                                          sensor01_ * 0.8f));
        g.fillEllipse(w);
        g.setColour(kInk);
        g.drawEllipse(w, 4.0f);

        // Column numbers above the MUTE row, per the hardware print.
        g.setColour(kInk);
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        for (int i = 0; i < 5; ++i)
            g.drawText(juce::String(i + 1),
                       frac(0.095 + 0.123 * i, 0.125, 0.115, 0.06),
                       juce::Justification::centred);

        // MUTE / TUNE / MOD row markers, rotated along the left edge like the
        // print (reads bottom-to-top).
        g.setColour(kInk.withAlpha(0.8f));
        g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
        auto rowMarker = [&](const juce::String& t, double fy, double fh)
        {
            const auto area = frac(0.005, fy, 0.085, fh);
            juce::Graphics::ScopedSaveState ss(g);
            g.addTransform(juce::AffineTransform::rotation(
                -juce::MathConstants<float>::halfPi,
                (float) area.getCentreX(), (float) area.getCentreY()));
            g.drawText(t, area.withSizeKeepingCentre(area.getHeight(), area.getWidth()),
                       juce::Justification::centred);
        };
        rowMarker("MUTE", 0.19, 0.11);
        rowMarker("TUNE", 0.34, 0.24);
        rowMarker("MOD", 0.635, 0.115);
    }

    std::unique_ptr<PushButton> mod[5], mute[5], hold;
    std::unique_ptr<LabeledKnob> tune[5], volt, att, rls;
    juce::Rectangle<int> ledBar_, sensor_;
    float gens_[5] = {}, sensor01_ = 0.0f;
};

// Papa Srapa voice (DRONE 3/6): rate/mod/divider/PITCH row, NOISE, S&H field.
class PapaSrapaSection : public Section
{
public:
    PapaSrapaSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title, Band::Top, id.substring(1))
    {
        badgeFx_ = 0.9575; // the S&H field + out jack own the true corner
        badgeFy_ = 0.665;
        rate = std::make_unique<LabeledKnob>(s, id + ".rate", "rate", kKnobBlue);
        fmamt = std::make_unique<LabeledKnob>(s, id + ".fmamt", "mod", kKnobBlue);
        divider = std::make_unique<LabeledKnob>(s, id + ".divider", "divider", kKnobBlue);
        pitch = std::make_unique<LabeledKnob>(s, id + ".pitch", "PITCH", kKnobBlue);
        noise = std::make_unique<LabeledKnob>(s, id + ".noise", "NOISE", kKnobBlue);
        fm = std::make_unique<SlideSwitch>(s, id + ".fm", "fm");
        am = std::make_unique<SlideSwitch>(s, id + ".am", "am");
        x10 = std::make_unique<SlideSwitch>(s, id + ".x10", "x10");
        hi = std::make_unique<SlideSwitch>(s, id + ".hi", "hi/low");
        hold = std::make_unique<PushButton>(s, id + ".hold", "HOLD", juce::Colour(0xffe8c33a));
        att = std::make_unique<LabeledKnob>(s, id + ".att", "ATT", kKnobBlue);
        rls = std::make_unique<LabeledKnob>(s, id + ".rls", "RLS", kKnobBlue);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 rate.get(), fmamt.get(), divider.get(), pitch.get(), noise.get(),
                 fm.get(), am.get(), x10.get(), hi.get(), hold.get(), att.get(), rls.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        place(*x10, 0.05, 0.14, 0.16, 0.10);
        place(*fm, 0.30, 0.14, 0.12, 0.10);
        place(*am, 0.48, 0.14, 0.12, 0.10);
        place(*hi, 0.68, 0.14, 0.18, 0.10);

        place(*rate, 0.03, 0.25, 0.18, 0.27);
        place(*fmamt, 0.23, 0.25, 0.18, 0.27);
        place(*divider, 0.43, 0.25, 0.18, 0.27); // hardware: a knob, not a dropdown
        place(*pitch, 0.65, 0.24, 0.26, 0.29);

        place(*noise, 0.72, 0.55, 0.20, 0.22);

        place(*hold, 0.115, 0.775, 0.085, 0.155);
        place(*att, 0.21, 0.74, 0.105, 0.21);
        place(*rls, 0.315, 0.74, 0.105, 0.21);

        cvLed_ = frac(0.53, 0.52, 0.05, 0.06);
        shBox_ = frac(0.575, 0.755, 0.37, 0.21);
    }

    void setTelemetry(float cvLed)
    {
        if (std::abs(cvLed - cv01_) > 0.05f)
        {
            cv01_ = cvLed;
            repaint(cvLed_.expanded(8));
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Modulator activity LED next to the cv out jack.
        led(g, cvLed_.getCentre().toFloat(), cv01_, kAccentRed);

        // S&H field outline.
        g.setColour(kInk);
        g.drawRoundedRectangle(shBox_.toFloat(), 10.0f, 4.0f);
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.setColour(kAccentRed);
        g.drawText("S&H", shBox_.withTrimmedBottom((int) (shBox_.getHeight() * 0.62)),
                   juce::Justification::centred);
    }

    std::unique_ptr<LabeledKnob> rate, fmamt, divider, pitch, noise, att, rls;
    std::unique_ptr<SlideSwitch> fm, am, x10, hi;
    std::unique_ptr<PushButton> hold;
    juce::Rectangle<int> cvLed_, shBox_;
    float cv01_ = 0.0f;
};

// VCO A/B: cv amt + tune flanking, big morphing-waveform rotary, pwm/pw, jack row.
class VcoSection : public Section
{
public:
    VcoSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title, Band::Top)
    {
        cvamt = std::make_unique<LabeledKnob>(s, id + ".cvamt", "cv amt", kKnobGreen);
        tune = std::make_unique<LabeledKnob>(s, id + ".tune", "tune", kKnobGreen);
        wave = std::make_unique<LabeledKnob>(s, id + ".wave", "", kKnobGreen);
        pwm = std::make_unique<LabeledKnob>(s, id + ".pwm", "pwm", kKnobGreen);
        pw = std::make_unique<LabeledKnob>(s, id + ".pw", "pw", kKnobGreen);
        oct = std::make_unique<SlideSwitch>(s, id + ".oct", "oct+3");
        sub = std::make_unique<SlideSwitch>(s, id + ".sub", "-1 sub");
        exp = std::make_unique<SlideSwitch>(s, id + ".exp", "lin/exp");
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 cvamt.get(), tune.get(), wave.get(), pwm.get(), pw.get(),
                 oct.get(), sub.get(), exp.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        place(*cvamt, 0.04, 0.14, 0.23, 0.26);
        place(*oct, 0.29, 0.15, 0.20, 0.11);
        place(*sub, 0.51, 0.15, 0.20, 0.11);
        place(*tune, 0.72, 0.14, 0.23, 0.26);
        place(*exp, 0.04, 0.42, 0.21, 0.10);
        place(*wave, 0.31, 0.27, 0.38, 0.46);
        // pwm/pw end above the census jack row (nut tops ~0.80) so the
        // LabeledKnob caption strip stays visible.
        place(*pwm, 0.04, 0.53, 0.23, 0.25);
        place(*pw, 0.72, 0.53, 0.23, 0.25);
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.drawText("MORPHING WAVEFORM", frac(0.28, 0.105, 0.44, 0.05),
                   juce::Justification::centred);
    }

    std::unique_ptr<LabeledKnob> cvamt, tune, wave, pwm, pw;
    std::unique_ptr<SlideSwitch> oct, sub, exp;
};

// Envelope A/B: hold/loop, A D S R, jack row (with the VCO dry outs).
class EnvelopeSection : public Section
{
public:
    EnvelopeSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title, Band::Top)
    {
        const char* n[] = { "A", "D", "S", "R" };
        const char* p[] = { ".a", ".d", ".s", ".r" };
        for (int i = 0; i < 4; ++i)
        {
            adsr[i] = std::make_unique<LabeledKnob>(s, id + p[i], n[i], kKnobGreen);
            addAndMakeVisible(*adsr[i]);
        }
        hold = std::make_unique<PushButton>(s, id + ".hold", "hold", juce::Colour(0xffe8c33a));
        loop = std::make_unique<PushButton>(s, id + ".loop", "loop", kAccentRed);
        addAndMakeVisible(*hold);
        addAndMakeVisible(*loop);
    }

    void resized() override
    {
        // Clear of the 64-unit title band above AND of the jack row's
        // above-the-nut labels (jacks at fy 0.78 print at ~fy 0.55-0.63;
        // envelope B's leftmost jack label reaches into this column).
        place(*hold, 0.02, 0.19, 0.10, 0.16);
        place(*loop, 0.02, 0.37, 0.10, 0.16);
        for (int i = 0; i < 4; ++i)
            place(*adsr[i], 0.17 + 0.205 * i, 0.24, 0.19, 0.36);
    }

private:
    std::unique_ptr<LabeledKnob> adsr[4];
    std::unique_ptr<PushButton> hold, loop;
};

// Dual Polivoks filter strip: 13 columns, CV L / CV R jacks in slots 4 and 8.
class FilterSection : public Section
{
public:
    explicit FilterSection(Apvts& s) : Section("", Band::None)
    {
        freqL = std::make_unique<LabeledKnob>(s, "filt.freqL", "FREQ", kKnobOrange);
        resL = std::make_unique<LabeledKnob>(s, "filt.resL", "RES", kKnobOrange);
        bpL = std::make_unique<SlideSwitch>(s, "filt.bpL", "BP");
        modL = std::make_unique<LabeledKnob>(s, "filt.modL", "MOD L", kKnobOrange);
        dist = std::make_unique<LabeledKnob>(s, "filt.dist", "DIST", kKnobOrange);
        link = std::make_unique<SlideSwitch>(s, "filt.link", "link");
        gain = std::make_unique<LabeledKnob>(s, "filt.gain", "GAIN", kKnobOrange);
        modR = std::make_unique<LabeledKnob>(s, "filt.modR", "MOD R", kKnobOrange);
        freqR = std::make_unique<LabeledKnob>(s, "filt.freqR", "FREQ", kKnobOrange);
        resR = std::make_unique<LabeledKnob>(s, "filt.resR", "RES", kKnobOrange);
        bpR = std::make_unique<SlideSwitch>(s, "filt.bpR", "BP");
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 freqL.get(), resL.get(), bpL.get(), modL.get(), dist.get(), link.get(),
                 gain.get(), modR.get(), freqR.get(), resR.get(), bpR.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        // Slots 4 and 8 stay empty — the CV L / CV R jacks live there. The
        // 13-slot row is inset so the rotated FILTER L/R side tags get their
        // own margin instead of colliding with the outer FREQ knobs.
        juce::Component* slots[13] = { freqL.get(), resL.get(), bpL.get(), modL.get(),
                                       nullptr, dist.get(), link.get(), gain.get(),
                                       nullptr, modR.get(), bpR.get(), freqR.get(), resR.get() };
        for (int i = 0; i < 13; ++i)
        {
            if (slots[i] == nullptr)
                continue;
            const bool sw = dynamic_cast<SlideSwitch*>(slots[i]) != nullptr;
            place(*slots[i], 0.028 + (i + (sw ? 0.18 : 0.02)) * 0.945 / 13.0,
                  sw ? 0.34 : 0.13, (sw ? 0.66 : 0.94) * 0.945 / 13.0, sw ? 0.32 : 0.72);
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Rotated side tags, like the render's vertical FILTER L / FILTER R.
        g.setColour(kInk);
        g.setFont(juce::FontOptions(34.0f, juce::Font::bold));
        for (int side = 0; side < 2; ++side)
        {
            const auto box = frac(side == 0 ? 0.0 : 0.972, 0.08, 0.028, 0.84).toFloat();
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(
                -juce::MathConstants<float>::halfPi, box.getCentreX(), box.getCentreY()));
            g.drawText(side == 0 ? "FILTER L" : "FILTER R",
                       juce::Rectangle<float>(box.getCentreX() - 95.0f,
                                              box.getCentreY() - 20.0f, 190.0f, 40.0f),
                       juce::Justification::centred);
            g.restoreState();
        }

        // BP above / LP below each mode switch (the hardware prints the two
        // response icons there; the glyphs proper are the P3 art pass).
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        for (int slot : { 2, 10 })
        {
            const auto sw = frac(0.028 + (slot + 0.18) * 0.945 / 13.0, 0.34,
                                 0.66 * 0.945 / 13.0, 0.32);
            g.setColour(kInk.withAlpha(0.85f));
            g.drawText("BP", sw.withY(sw.getY() - 34).withHeight(30),
                       juce::Justification::centred);
            g.drawText("LP", sw.withY(sw.getBottom() + 4).withHeight(30),
                       juce::Justification::centred);
        }
    }

    std::unique_ptr<LabeledKnob> freqL, resL, modL, dist, gain, modR, freqR, resR;
    std::unique_ptr<SlideSwitch> bpL, link, bpR;
};

// 10-channel voice mixer: PAN over VOL per channel; PAN is the filter routing.
// Hardware print: "PAN ◆" caption + L◄ ►R marks above, black channel-name band
// between the rows, "VOL" caption over the grey knobs; the "VOICE MIXER" title
// itself is the small center badge between the envelopes (PanelView).
class MixerSection : public Section
{
public:
    explicit MixerSection(Apvts& s) : Section("VOICE MIXER", Band::None)
    {
        const char* ids[] = { "d1", "d2", "d3", "ext", "vcoA", "vcoB", "pre", "d4", "d5", "d6" };
        for (int c = 0; c < 10; ++c)
        {
            const auto base = "mix." + juce::String(ids[c]);
            pan[c] = std::make_unique<LabeledKnob>(s, base + ".pan", "", kKnobBlack);
            vol[c] = std::make_unique<LabeledKnob>(s, base + ".vol", "", kKnobGrey);
            addAndMakeVisible(*pan[c]);
            addAndMakeVisible(*vol[c]);
        }
    }

    void resized() override
    {
        for (int c = 0; c < 10; ++c)
        {
            place(*pan[c], 0.004 + 0.0996 * c, 0.14, 0.092, 0.28);
            place(*vol[c], 0.004 + 0.0996 * c, 0.685, 0.092, 0.30);
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        static const char* names[] = { "DRONE 1", "DRONE 2", "DRONE 3", "EXT.AUDIO",
                                       "VCO A", "VCO B", "PREAMP", "DRONE 4",
                                       "DRONE 5", "DRONE 6" };
        // Cell separators first (the name band paints over their middle).
        g.setColour(kInk.withAlpha(0.28f));
        for (int c = 1; c < 10; ++c)
            g.drawVerticalLine(frac(0.004 + 0.0996 * c, 0.0, 0.0, 0.0).getX() - 2,
                               (float) frac(0.0, 0.05, 0.0, 0.0).getY(),
                               (float) frac(0.0, 0.96, 0.0, 0.0).getY());

        const auto diamond = [&g](juce::Point<float> c, float r)
        {
            juce::Path p;
            p.addQuadrilateral(c.x, c.y - r, c.x + r, c.y, c.x, c.y + r, c.x - r, c.y);
            g.fillPath(p);
        };
        const auto arrow = [&g](juce::Point<float> tip, float dir, float r)
        {
            juce::Path p;
            p.addTriangle(tip.x + dir * r, tip.y,
                          tip.x, tip.y - r * 0.8f, tip.x, tip.y + r * 0.8f);
            g.fillPath(p);
        };

        g.setColour(kInk);
        for (int c = 0; c < 10; ++c)
        {
            const auto cell = frac(0.004 + 0.0996 * c, 0.0, 0.092, 1.0);
            // "PAN ◆" over the black knob.
            g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
            g.drawText("PAN", cell.withY(frac(0.0, 0.035, 0.0, 0.075).getY())
                                  .withHeight(frac(0.0, 0.0, 0.0, 0.075).getHeight())
                                  .translated(-12, 0),
                       juce::Justification::centred);
            diamond({ (float) cell.getCentreX() + 34.0f,
                      (float) frac(0.0, 0.073, 0.0, 0.0).getY() }, 9.0f);
            // L◄ ►R at the pan cell's lower corners.
            const float ly = (float) frac(0.0, 0.47, 0.0, 0.0).getY();
            g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
            g.drawText("L", cell.getX() + 6, (int) ly - 14, 22, 28, juce::Justification::centred);
            g.drawText("R", cell.getRight() - 28, (int) ly - 14, 22, 28, juce::Justification::centred);
            arrow({ (float) cell.getX() + 30.0f, ly }, -1.0f, 8.0f);
            arrow({ (float) cell.getRight() - 30.0f, ly }, 1.0f, 8.0f);
        }

        // Black channel-name band between PAN and VOL rows.
        const auto band = frac(0.006, 0.53, 0.988, 0.115).toFloat();
        g.setColour(kInk);
        g.fillRoundedRectangle(band, 6.0f);
        g.setColour(kCream);
        for (int c = 0; c < 10; ++c)
        {
            const auto cell = frac(0.004 + 0.0996 * c, 0.53, 0.092, 0.115);
            g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
            g.drawFittedText(names[c], cell.reduced(4, 0), juce::Justification::centred, 1);
        }

        // "VOL" over each grey knob.
        g.setColour(kInk);
        g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
        for (int c = 0; c < 10; ++c)
            g.drawText("VOL", frac(0.004 + 0.0996 * c, 0.632, 0.092, 0.05),
                       juce::Justification::centred);
    }

    std::unique_ptr<LabeledKnob> pan[10], vol[10];
};

// DUAL EFFECTOR: cv x/y/z jacks over X/Y/Z knobs, cartridge slot with the two
// 1-2-3 program toggles, BLEND, MASTER VOLUME.
class EffectorSection : public Section
{
public:
    explicit EffectorSection(Apvts& s) : Section("DUAL EFFECTOR")
    {
        cart = std::make_unique<ChoiceBox>(s, "fx.cart", "");
        progL = std::make_unique<Prog3Switch>(s, "fx.progL", "L");
        progR = std::make_unique<Prog3Switch>(s, "fx.progR", "R");
        x = std::make_unique<LabeledKnob>(s, "fx.x", "X", kKnobOrange);
        y = std::make_unique<LabeledKnob>(s, "fx.y", "Y", kKnobOrange);
        z = std::make_unique<LabeledKnob>(s, "fx.z", "Z", kKnobOrange);
        blend = std::make_unique<LabeledKnob>(s, "fx.blend", "BLEND", kKnobOrange);
        master = std::make_unique<LabeledKnob>(s, "master.vol", "MASTER VOLUME", kKnobOrange);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 cart.get(), progL.get(), progR.get(), x.get(), y.get(), z.get(),
                 blend.get(), master.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        // X/Y/Z sit below their CV jacks (fy 0.33 in the census) with room
        // for the jack labels in between.
        place(*x, 0.005, 0.52, 0.08, 0.40);
        place(*y, 0.085, 0.52, 0.08, 0.40);
        place(*z, 0.165, 0.52, 0.08, 0.40);
        place(*progL, 0.25, 0.24, 0.085, 0.52);
        place(*cart, 0.36, 0.29, 0.245, 0.36);
        place(*progR, 0.625, 0.24, 0.085, 0.52);
        place(*blend, 0.725, 0.30, 0.10, 0.44);
        place(*master, 0.83, 0.22, 0.16, 0.60);
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Cartridge bay: a slot frame around the inserted-cartridge selector,
        // flanked by the two 1-2-3 program toggles.
        const auto bay = frac(0.345, 0.20, 0.275, 0.56).toFloat();
        g.setColour(juce::Colour(0xff2b2b30));
        g.fillRoundedRectangle(bay, 12.0f);
        g.setColour(kInk);
        g.drawRoundedRectangle(bay, 12.0f, 4.0f);
        g.setColour(kCream.withAlpha(0.85f));
        g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
        g.drawText("CARTRIDGE", bay.withTrimmedBottom(bay.getHeight() * 0.74f),
                   juce::Justification::centred);
        g.setColour(kCream.withAlpha(0.6f));
        g.setFont(juce::FontOptions(27.0f, juce::Font::bold));
        g.drawText("flip 1-2-3 to load", bay.withTrimmedTop(bay.getHeight() * 0.76f),
                   juce::Justification::centred);

        g.setColour(kAccentRed);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText("1-2-3", frac(0.25, 0.79, 0.085, 0.08), juce::Justification::centred);
        g.drawText("1-2-3", frac(0.625, 0.79, 0.085, 0.08), juce::Justification::centred);
    }

    std::unique_ptr<ChoiceBox> cart;
    std::unique_ptr<Prog3Switch> progL, progR;
    std::unique_ptr<LabeledKnob> x, y, z, blend, master;
};

// ---------------------------------------------------------------- mod strip

class LfoSection : public Section
{
public:
    LfoSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title, Band::Bottom)
    {
        wave = std::make_unique<LabeledKnob>(s, id + ".wave", "wave", kKnobRed);
        rate = std::make_unique<LabeledKnob>(s, id + ".rate", "rate", kKnobRed);
        // Hardware: 3-position range switch printed x6 / x1 / x10 top-down
        // (x1 is the centre detent); choice order is x1, x6, x10.
        range = std::make_unique<ChoiceSlideSwitch>(s, id + ".range",
                                                    juce::StringArray { "x6", "x1", "x10" },
                                                    std::vector<int> { 1, 0, 2 });
        addAndMakeVisible(*wave);
        addAndMakeVisible(*rate);
        addAndMakeVisible(*range);
    }

    void resized() override
    {
        place(*wave, 0.03, 0.10, 0.27, 0.62);
        place(*range, 0.50, 0.10, 0.20, 0.62);
        place(*rate, 0.72, 0.10, 0.27, 0.62);
        led_ = frac(0.40, 0.16, 0.0, 0.0).withSize(1, 1); // clear of the out jack's label
    }

    void setTelemetry(float level)
    {
        if (std::abs(level - level01_) > 0.05f)
        {
            level01_ = level;
            repaint(juce::Rectangle<int>(led_.getX() - 24, led_.getY() - 24, 48, 48));
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        led(g, led_.getPosition().toFloat(), level01_, juce::Colour(0xff4fa3e0));
    }

    std::unique_ptr<LabeledKnob> wave, rate;
    std::unique_ptr<ChoiceSlideSwitch> range;
    juce::Rectangle<int> led_;
    float level01_ = 0.0f;
};

class JoystickSection : public Section
{
public:
    explicit JoystickSection(Apvts& s) : Section("JOYSTICK", Band::Bottom)
    {
        xoff = std::make_unique<LabeledKnob>(s, "joy.xoff", "offset X", kKnobRed);
        yoff = std::make_unique<LabeledKnob>(s, "joy.yoff", "offset Y", kKnobRed);
        addAndMakeVisible(*xoff);
        addAndMakeVisible(*yoff);
    }

    void resized() override
    {
        place(*xoff, 0.02, 0.10, 0.26, 0.62);
        place(*yoff, 0.72, 0.10, 0.26, 0.62);
    }

private:
    std::unique_ptr<LabeledKnob> xoff, yoff;
};

class SeqSection : public Section
{
public:
    explicit SeqSection(Apvts& s) : Section("5 STEP SEQ.  VOLTAGE", Band::Bottom)
    {
        pulser = std::make_unique<LabeledKnob>(s, "seq.pulser", "pulser", kKnobRed);
        // Hardware: 3-position switch printed 4 / 5 / 3 top-down (5 = centre).
        stages = std::make_unique<ChoiceSlideSwitch>(s, "seq.stages",
                                                     juce::StringArray { "4", "5", "3" },
                                                     std::vector<int> { 1, 2, 0 }, "stages");
        addAndMakeVisible(*pulser);
        addAndMakeVisible(*stages);
        for (int i = 0; i < 5; ++i)
        {
            const auto n = juce::String(i + 1);
            step[i] = std::make_unique<LabeledKnob>(s, "seq.step" + n, "step " + n, kKnobRed);
            gate[i] = std::make_unique<SlideSwitch>(s, "seq.gate" + n, "");
            addAndMakeVisible(*step[i]);
            addAndMakeVisible(*gate[i]);
        }
    }

    void resized() override
    {
        place(*pulser, 0.005, 0.10, 0.085, 0.62);
        place(*stages, 0.135, 0.10, 0.075, 0.66);
        for (int i = 0; i < 5; ++i)
        {
            const double x = 0.225 + 0.106 * i;
            place(*step[i], x, 0.16, 0.072, 0.56);
            place(*gate[i], x + 0.074, 0.28, 0.028, 0.32);
            leds_[i] = frac(x + 0.036, 0.10, 0.0, 0.0).getPosition();
        }
    }

    void setTelemetry(int stepIdx, float gateHigh)
    {
        if (stepIdx != step_ || std::abs(gateHigh - gate01_) > 0.5f)
        {
            step_ = stepIdx;
            gate01_ = gateHigh;
            repaint(frac(0.2, 0.06, 0.6, 0.12));
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Pulser -> clock routing hint (starts clear of the pulser knob).
        g.setColour(kAccentRed.withAlpha(0.85f));
        g.drawArrow(juce::Line<float>((float) frac(0.094, 0.20, 0, 0).getX(),
                                      (float) frac(0, 0.20, 0, 0).getY(),
                                      (float) frac(0.110, 0.30, 0, 0).getX(),
                                      (float) frac(0, 0.32, 0, 0).getY()),
                    4.0f, 14.0f, 14.0f);
        for (int i = 0; i < 5; ++i)
            led(g, leds_[i].toFloat(), i == step_ ? (0.35f + 0.65f * gate01_) : 0.0f,
                kAccentRed, 13.0f);
    }

    std::unique_ptr<LabeledKnob> pulser, step[5];
    std::unique_ptr<ChoiceSlideSwitch> stages;
    std::unique_ptr<SlideSwitch> gate[5];
    juce::Point<int> leds_[5];
    int step_ = 0;
    float gate01_ = 0.0f;
};

class PreampSection : public Section
{
public:
    explicit PreampSection(Apvts& s) : Section("PREAMP", Band::Bottom)
    {
        gain = std::make_unique<LabeledKnob>(s, "pre.gain", "gain", kKnobRed);
        addAndMakeVisible(*gain);
    }

    void resized() override { place(*gain, 0.42, 0.10, 0.36, 0.62); }

    void setTelemetry(float clip)
    {
        if (std::abs(clip - clip01_) > 0.5f)
        {
            clip01_ = clip;
            repaint(frac(0.80, 0.20, 0.14, 0.22));
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        led(g, frac(0.87, 0.30, 0, 0).getPosition().toFloat(), clip01_, kAccentRed);
    }

    std::unique_ptr<LabeledKnob> gain;
    float clip01_ = 0.0f;
};

class EnvFollowerSection : public Section
{
public:
    explicit EnvFollowerSection(Apvts& s) : Section("ENVELOPE FOLLOWER", Band::Bottom)
    {
        att = std::make_unique<LabeledKnob>(s, "envf.att", "attack", kKnobRed);
        rel = std::make_unique<LabeledKnob>(s, "envf.rel", "release", kKnobRed);
        addAndMakeVisible(*att);
        addAndMakeVisible(*rel);
    }

    void resized() override
    {
        place(*att, 0.02, 0.10, 0.23, 0.62);
        place(*rel, 0.27, 0.10, 0.23, 0.62);
    }

    void setTelemetry(float env, float gateHigh)
    {
        if (std::abs(env - env01_) > 0.04f || std::abs(gateHigh - gate01_) > 0.5f)
        {
            env01_ = env;
            gate01_ = gateHigh;
            repaint(frac(0.56, 0.08, 0.42, 0.20));
        }
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        led(g, frac(0.64, 0.16, 0, 0).getPosition().toFloat(), env01_, kAccentRed);
        led(g, frac(0.875, 0.16, 0, 0).getPosition().toFloat(), gate01_,
            juce::Colour(0xff3ac86a));
    }

    std::unique_ptr<LabeledKnob> att, rel;
    float env01_ = 0.0f, gate01_ = 0.0f;
};

// ---------------------------------------------------------------- performance zone

// The physical position-locking stick (bottom-left on the hardware) — drives
// the joy.x / joy.y parameters; the mod-strip block holds the offset knobs.
class JoyPad : public juce::Component
{
public:
    JoyPad(Apvts& s, const juce::String& xId, const juce::String& yId)
        : xAtt(*s.getParameter(xId), [this](float v) { x_ = v; repaint(); }),
          yAtt(*s.getParameter(yId), [this](float v) { y_ = v; repaint(); })
    {
        xAtt.sendInitialUpdate();
        yAtt.sendInitialUpdate();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        xAtt.beginGesture();
        yAtt.beginGesture();
        mouseDrag(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const auto r = pad();
        xAtt.setValueAsPartOfGesture(juce::jlimit(-1.0f, 1.0f,
            ((float) e.x - r.getCentreX()) / (r.getWidth() * 0.5f - 40.0f)));
        yAtt.setValueAsPartOfGesture(juce::jlimit(-1.0f, 1.0f,
            (r.getCentreY() - (float) e.y) / (r.getHeight() * 0.5f - 40.0f)));
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        xAtt.endGesture();
        yAtt.endGesture(); // position-locking: the stick stays where released
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = pad();

        g.setColour(kInk.withAlpha(0.8f));
        for (int i = 0; i < 4; ++i) // N E S W direction arrows
        {
            juce::Path a;
            a.addTriangle(-16.0f, 12.0f, 16.0f, 12.0f, 0.0f, -14.0f);
            const float ang = juce::MathConstants<float>::halfPi * (float) i;
            g.fillPath(a, juce::AffineTransform::rotation(ang)
                              .translated(r.getCentre().getPointOnCircumference(
                                  r.getWidth() * 0.5f - 10.0f, ang)));
        }

        const auto c = juce::Point<float>(
            r.getCentreX() + x_ * (r.getWidth() * 0.5f - 40.0f),
            r.getCentreY() - y_ * (r.getHeight() * 0.5f - 40.0f));
        g.setColour(juce::Colour(0xff2b2b30));
        g.fillEllipse(c.x - 68.0f, c.y - 68.0f, 136.0f, 136.0f);
        g.setColour(juce::Colour(0xff47474d));
        g.fillEllipse(c.x - 40.0f, c.y - 40.0f, 80.0f, 80.0f);
    }

private:
    juce::Rectangle<float> pad() const
    {
        return getLocalBounds().toFloat().reduced(30.0f);
    }

    float x_ = 0.0f, y_ = 0.0f;
    juce::ParameterAttachment xAtt, yAtt;
};

// DRONE VOICES keypad: 6 gates in the hardware's column order 1,4 / 2,5 / 3,6.
class KeypadSection : public juce::Component
{
public:
    explicit KeypadSection(Apvts& s)
    {
        const char* ids[6] = { "d1", "d4", "d2", "d5", "d3", "d6" }; // row-major
        for (int i = 0; i < 6; ++i)
        {
            keys[i] = std::make_unique<PushButton>(s, juce::String(ids[i]) + ".gate",
                                                   juce::String(ids[i]).substring(1),
                                                   juce::Colours::white);
            addAndMakeVisible(*keys[i]);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(52.0f, juce::Font::bold));
        g.drawText("DRONE VOICES", getLocalBounds().removeFromTop(70),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop(90);
        const int bw = r.getWidth() / 2, bh = r.getHeight() / 3;
        for (int i = 0; i < 6; ++i)
            keys[i]->setBounds(juce::Rectangle<int>(r.getX() + (i % 2) * bw,
                                                    r.getY() + (i / 2) * bh, bw, bh)
                                   .reduced(24));
    }

private:
    std::unique_ptr<PushButton> keys[6];
};

} // namespace solar
