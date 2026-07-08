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

// ------------------------------------------------------------- print art kit
// Shared silkscreen art (M9b P3). All coordinates are logical panel units.

// Curved routing arrow with the print's tail dot; the head lands on b,
// oriented along the curve's end tangent. Caller sets the colour.
inline void printArrow(juce::Graphics& g, juce::Point<float> a, juce::Point<float> c1,
                       juce::Point<float> c2, juce::Point<float> b,
                       float thickness = 5.0f, float head = 18.0f)
{
    juce::Path p;
    p.startNewSubPath(a);
    p.cubicTo(c1, c2, b);
    g.strokePath(p, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));
    const auto dir = b - c2;
    juce::Path h;
    h.addTriangle(0.0f, 0.0f, -head, head * 0.45f, -head, -head * 0.45f);
    g.fillPath(h, juce::AffineTransform::rotation(std::atan2(dir.y, dir.x)).translated(b));
    g.fillEllipse(a.x - 7.0f, a.y - 7.0f, 14.0f, 14.0f);
}

// Numbered panel screw — the 42N print scatters slotted screw heads with
// position numbers over the VCO sections (1-7 on A, 8-14 on B).
inline void printScrew(juce::Graphics& g, juce::Point<float> c, int number,
                       juce::Point<float> numberAt)
{
    g.setColour(juce::Colour(0xffcfccc2));
    g.fillEllipse(c.x - 13.0f, c.y - 13.0f, 26.0f, 26.0f);
    g.setColour(kInk);
    g.drawEllipse(c.x - 13.0f, c.y - 13.0f, 26.0f, 26.0f, 2.5f);
    g.drawLine(c.x - 8.0f, c.y + 8.0f, c.x + 8.0f, c.y - 8.0f, 3.0f);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText(juce::String(number),
               juce::Rectangle<float>(48.0f, 30.0f).withCentre(numberAt),
               juce::Justification::centred);
}

// The morph knob's waveform ring, left to right: sine, triangle, saw, pulse,
// crossed sines, overtone scribble (the print's 6 morph stations).
inline juce::Path waveGlyph(int kind)
{
    juce::Path p;
    switch (kind)
    {
        case 0: // sine
            p.startNewSubPath(-22.0f, 0.0f);
            p.cubicTo(-15.0f, -19.0f, -7.0f, -19.0f, 0.0f, 0.0f);
            p.cubicTo(7.0f, 19.0f, 15.0f, 19.0f, 22.0f, 0.0f);
            break;
        case 1: // triangle
            p.startNewSubPath(-22.0f, 10.0f);
            p.lineTo(-11.0f, -12.0f);
            p.lineTo(0.0f, 10.0f);
            p.lineTo(11.0f, -12.0f);
            p.lineTo(22.0f, 10.0f);
            break;
        case 2: // saw
            p.startNewSubPath(-22.0f, 12.0f);
            p.lineTo(-2.0f, -12.0f);
            p.lineTo(-2.0f, 12.0f);
            p.lineTo(18.0f, -12.0f);
            p.lineTo(18.0f, 12.0f);
            break;
        case 3: // pulse
            p.startNewSubPath(-22.0f, 12.0f);
            p.lineTo(-22.0f, -12.0f);
            p.lineTo(-4.0f, -12.0f);
            p.lineTo(-4.0f, 12.0f);
            p.lineTo(14.0f, 12.0f);
            p.lineTo(14.0f, -12.0f);
            p.lineTo(20.0f, -12.0f);
            break;
        case 4: // crossed sines (the morph's mid stations)
            p.startNewSubPath(-20.0f, -12.0f);
            p.cubicTo(-6.0f, -12.0f, 6.0f, 12.0f, 20.0f, 12.0f);
            p.startNewSubPath(-20.0f, 12.0f);
            p.cubicTo(-6.0f, 12.0f, 6.0f, -12.0f, 20.0f, -12.0f);
            break;
        default: // gnarly overtone scribble
            p.startNewSubPath(-20.0f, 6.0f);
            p.lineTo(-14.0f, -8.0f);
            p.lineTo(-9.0f, 8.0f);
            p.lineTo(-4.0f, -10.0f);
            p.lineTo(1.0f, 10.0f);
            p.lineTo(6.0f, -6.0f);
            p.lineTo(11.0f, 8.0f);
            p.lineTo(16.0f, -4.0f);
            p.lineTo(20.0f, 4.0f);
            break;
    }
    return p;
}

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

// Round push button with an LED dot (MUTE / MOD / HOLD).
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
    // Band styles follow the print: full-width Top for the mod strip's
    // Bottom, short TopRight chips on the voice sections (DRONE 3/6, VCOs),
    // TopLeft on the envelopes, a centred chip on the effector, None where
    // the hardware prints no title (classic drones, filter, mixer).
    enum class Band { Top, TopRight, TopLeft, TopCentre, Bottom, None };

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
            auto band = band_ == Band::Bottom ? r.removeFromBottom((float) kBand)
                                              : r.removeFromTop((float) kBand);
            if (band_ == Band::TopRight)
                band = band.removeFromRight(band.getWidth() * 0.40f);
            else if (band_ == Band::TopLeft)
                band = band.removeFromLeft(band.getWidth() * 0.62f);
            else if (band_ == Band::TopCentre)
                band = band.withSizeKeepingCentre(band.getWidth() * 0.36f, band.getHeight());
            g.fillRoundedRectangle(band.reduced(5.0f), 14.0f);
            g.setColour(kCream);
            g.setFont(juce::FontOptions((float) kBand * 0.62f, juce::Font::bold));
            g.drawText(title_, band.reduced(24.0f, 0.0f),
                       band_ == Band::Top || band_ == Band::TopLeft
                           ? juce::Justification::centredLeft
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
        : Section(title, Band::None, id.substring(1)) // print has no title here
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
        // No title band (the print has none), so the rows use the full height.
        for (int g = 0; g < 5; ++g)
        {
            const double x = 0.095 + 0.123 * g;
            place(*mute[g], x, 0.125, 0.115, 0.11);
            place(*tune[g], x, 0.25, 0.115, 0.32);
            place(*mod[g], x, 0.615, 0.115, 0.115);
        }
        place(*volt, 0.755, 0.24, 0.19, 0.36);
        place(*hold, 0.125, 0.775, 0.09, 0.155);
        place(*att, 0.225, 0.74, 0.135, 0.21);
        place(*rls, 0.365, 0.74, 0.135, 0.21);

        ledBar_ = frac(0.73, 0.065, 0.24, 0.10);
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
        // Gen activity LED bar — red vertical segments like the print —
        // with the "1 2 3 4 5" print beneath.
        auto bar = ledBar_.toFloat();
        g.setColour(juce::Colour(0xff101013));
        g.fillRoundedRectangle(bar, 8.0f);
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        for (int i = 0; i < 5; ++i)
        {
            const float cx = bar.getX() + bar.getWidth() * ((float) i + 0.5f) / 5.0f;
            const float bw = bar.getWidth() / 5.0f * 0.36f;
            const float bh = bar.getHeight() * 0.70f;
            g.setColour(kAccentRed.withAlpha(
                0.16f + 0.84f * juce::jlimit(0.0f, 1.0f, gens_[i])));
            g.fillRect(cx - bw * 0.5f, bar.getCentreY() - bh * 0.5f, bw, bh);
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
                       frac(0.095 + 0.123 * i, 0.055, 0.115, 0.06),
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
        rowMarker("MUTE", 0.125, 0.11);
        rowMarker("TUNE", 0.28, 0.24);
        rowMarker("MOD", 0.615, 0.115);
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
        : Section(title, Band::TopRight, id.substring(1)) // print: short chip top-right
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

        cvLed_ = frac(0.502, 0.52, 0.05, 0.06);
        // Box bottom rides the section border like the print, so the
        // in/clock/out labels sit inside it un-struck.
        shBox_ = frac(0.575, 0.75, 0.37, 0.238);
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

        // P3 print art: "LFO" over the x10 range switch with the print's
        // dotted run, red routing arrows from the LFO rate into the fm / am
        // switches, and the noise spark glyph by the NOISE knob.
        g.setColour(kInk);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText("LFO", frac(0.025, 0.030, 0.12, 0.07), juce::Justification::centredLeft);
        {
            const float dy = (float) frac(0.0, 0.115, 0, 0).getY();
            g.setColour(kInk.withAlpha(0.7f));
            const float dashes[2] = { 6.0f, 8.0f };
            g.drawDashedLine({ (float) frac(0.03, 0.0, 0, 0).getX(), dy,
                               (float) frac(0.20, 0.0, 0, 0).getX(), dy },
                             dashes, 2, 3.0f);
        }

        g.setColour(kAccentRed.withAlpha(0.9f));
        printArrow(g, frac(0.160, 0.260, 0, 0).getPosition().toFloat(),
                   frac(0.190, 0.155, 0, 0).getPosition().toFloat(),
                   frac(0.240, 0.135, 0, 0).getPosition().toFloat(),
                   frac(0.295, 0.178, 0, 0).getPosition().toFloat(), 4.0f, 15.0f);
        printArrow(g, frac(0.175, 0.270, 0, 0).getPosition().toFloat(),
                   frac(0.250, 0.100, 0, 0).getPosition().toFloat(),
                   frac(0.380, 0.090, 0, 0).getPosition().toFloat(),
                   frac(0.475, 0.175, 0, 0).getPosition().toFloat(), 4.0f, 15.0f);

        g.setColour(kInk);
        {
            const auto sp = frac(0.51, 0.655, 0, 0).getPosition().toFloat();
            static constexpr float zig[10][2] = {
                { -32.0f, 6.0f },  { -24.0f, -10.0f }, { -17.0f, 12.0f },
                { -10.0f, -16.0f }, { -3.0f, 18.0f },  { 4.0f, -14.0f },
                { 11.0f, 12.0f },  { 18.0f, -9.0f },   { 25.0f, 7.0f },
                { 32.0f, -4.0f },
            };
            juce::Path spark;
            spark.startNewSubPath(sp.x + zig[0][0], sp.y + zig[0][1]);
            for (int i = 1; i < 10; ++i)
                spark.lineTo(sp.x + zig[i][0], sp.y + zig[i][1]);
            g.strokePath(spark, juce::PathStrokeType(3.0f));
        }

        // S&H field outline with the print's corner badge (in-box text would
        // hide behind the jack nuts).
        g.setColour(kInk);
        g.drawRoundedRectangle(shBox_.toFloat(), 10.0f, 4.0f);
        const auto chip = juce::Rectangle<float>(140.0f, 54.0f)
                              .withCentre({ (float) shBox_.getRight() - 70.0f,
                                            (float) shBox_.getY() + 4.0f });
        g.fillRoundedRectangle(chip, 10.0f);
        g.setColour(kCream);
        g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
        g.drawText("S&H", chip, juce::Justification::centred);
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
        : Section(title, Band::TopRight), // print: short chip top-right
          screwBase_(id == "vcoB" ? 8 : 1)
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
        // Knob sits low like the print so the P3 glyph ring clears the
        // oct+3 / -1 sub switch row above it.
        place(*wave, 0.31, 0.335, 0.38, 0.44);
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
        // "low" = the oct switch's down position (print sub-label).
        g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
        g.drawText("low", frac(0.27, 0.265, 0.10, 0.05), juce::Justification::centred);

        // Morph glyph ring (P3): six waveform stations on radial ticks
        // around the big knob, sine to overtone scribble like the print.
        auto wb = wave->getBounds().toFloat();
        wb.removeFromBottom(juce::jmax(30.0f, wb.getHeight() / 5.0f)); // caption strip
        const auto kc = wb.getCentre();
        const float rim = juce::jmin(wb.getWidth(), wb.getHeight()) * 0.5f;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::degreesToRadians(-75.0f + 30.0f * (float) i);
            g.drawLine(juce::Line<float>(kc.getPointOnCircumference(rim + 4.0f, a),
                                         kc.getPointOnCircumference(rim + 18.0f, a)),
                       3.5f);
            g.strokePath(waveGlyph(i),
                         juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded),
                         juce::AffineTransform::translation(
                             kc.getPointOnCircumference(rim + 42.0f, a)));
        }

        // Numbered screws (1-7 / 8-14), scattered like the print but nudged
        // into this layout's free panel between the controls.
        static constexpr struct { double fx, fy, nfx, nfy; } kScrews[7] = {
            { 0.315, 0.075, 0.268, 0.073 }, { 0.045, 0.115, 0.090, 0.113 },
            { 0.285, 0.520, 0.285, 0.572 }, { 0.710, 0.520, 0.710, 0.572 },
            { 0.835, 0.460, 0.835, 0.512 }, { 0.045, 0.790, 0.045, 0.842 },
            { 0.225, 0.905, 0.268, 0.903 },
        };
        for (int i = 0; i < 7; ++i)
            printScrew(g,
                       frac(kScrews[i].fx, kScrews[i].fy, 0, 0).getPosition().toFloat(),
                       screwBase_ + i,
                       frac(kScrews[i].nfx, kScrews[i].nfy, 0, 0).getPosition().toFloat());

        // Red routing arrow: the pwm jack's CV sweeps up into the pwm depth
        // knob (print). Curve stays under the morph knob.
        g.setColour(kAccentRed.withAlpha(0.9f));
        printArrow(g, frac(0.585, 0.820, 0, 0).getPosition().toFloat(),
                   frac(0.460, 0.865, 0, 0).getPosition().toFloat(),
                   frac(0.340, 0.760, 0, 0).getPosition().toFloat(),
                   frac(0.270, 0.660, 0, 0).getPosition().toFloat());

        // "-1 sub" pulse glyph after the switch caption, per print.
        g.setColour(kInk);
        juce::Path p;
        const auto sq = frac(0.7062, 0.205, 0, 0).getPosition().toFloat();
        p.startNewSubPath(sq.x - 14.0f, sq.y + 9.0f);
        p.lineTo(sq.x - 14.0f, sq.y - 9.0f);
        p.lineTo(sq.x + 14.0f, sq.y - 9.0f);
        p.lineTo(sq.x + 14.0f, sq.y + 9.0f);
        g.strokePath(p, juce::PathStrokeType(3.5f));
    }

    std::unique_ptr<LabeledKnob> cvamt, tune, wave, pwm, pw;
    std::unique_ptr<SlideSwitch> oct, sub, exp;
    const int screwBase_;
};

// Envelope A/B: hold/loop, A D S R, jack row (with the VCO dry outs).
class EnvelopeSection : public Section
{
public:
    EnvelopeSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title, Band::TopLeft) // print: band spans ~60%, text left
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
        // Clear of the 64-unit title band above and of the jack row below
        // (jacks at fy 0.72, ring tops ~0.62; labels print below the nuts
        // like the hardware).
        place(*hold, 0.02, 0.19, 0.10, 0.16);
        place(*loop, 0.02, 0.37, 0.10, 0.16);
        for (int i = 0; i < 4; ++i)
            place(*adsr[i], 0.17 + 0.205 * i, 0.22, 0.19, 0.36);
    }

private:
    std::unique_ptr<LabeledKnob> adsr[4];
    std::unique_ptr<PushButton> hold, loop;
};

// Dual Polivoks filter strip. Hardware print: FREQ RES · CV L DIST link GAIN
// CV R · FREQ RES, with the BP/LP toggle floating above each FREQ↔RES pair on
// a dotted bracket. MOD L/R belong to the effector body above (EffectorSection
// draws them; the params stay filt.*).
class FilterSection : public Section
{
public:
    explicit FilterSection(Apvts& s) : Section("", Band::None)
    {
        freqL = std::make_unique<LabeledKnob>(s, "filt.freqL", "FREQ", kKnobOrange);
        resL = std::make_unique<LabeledKnob>(s, "filt.resL", "RES", kKnobOrange);
        bpL = std::make_unique<SlideSwitch>(s, "filt.bpL", "");
        dist = std::make_unique<LabeledKnob>(s, "filt.dist", "DIST", kKnobOrange);
        link = std::make_unique<SlideSwitch>(s, "filt.link", "");
        gain = std::make_unique<LabeledKnob>(s, "filt.gain", "GAIN", kKnobOrange);
        freqR = std::make_unique<LabeledKnob>(s, "filt.freqR", "FREQ", kKnobOrange);
        resR = std::make_unique<LabeledKnob>(s, "filt.resR", "RES", kKnobOrange);
        bpR = std::make_unique<SlideSwitch>(s, "filt.bpR", "");
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 freqL.get(), resL.get(), bpL.get(), dist.get(), link.get(),
                 gain.get(), freqR.get(), resR.get(), bpR.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        // Columns re-slotted around the frozen CV L / CV R census jacks
        // (fx 4.5/13 ≈ 0.346 and 8.5/13 ≈ 0.654).
        const auto knob = [this](juce::Component& c, double cx)
        {
            place(c, cx - 0.0525, 0.16, 0.105, 0.74);
        };
        knob(*freqL, 0.095);
        knob(*resL, 0.215);
        knob(*dist, 0.425);
        knob(*gain, 0.575);
        knob(*freqR, 0.785);
        knob(*resR, 0.905);
        place(*bpL, 0.132, 0.02, 0.046, 0.30);
        place(*bpR, 0.822, 0.02, 0.046, 0.30);
        place(*link, 0.477, 0.22, 0.046, 0.32);
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

        // BP … LP dotted bracket over each FREQ↔RES pair, toggle in the
        // middle (response-curve glyphs land in P3).
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        const float dashes[2] = { 7.0f, 9.0f };
        for (int side = 0; side < 2; ++side)
        {
            const double fFreq = side == 0 ? 0.095 : 0.785;
            const double fRes = side == 0 ? 0.215 : 0.905;
            const float bx1 = (float) frac(fFreq, 0.0, 0.0, 0.0).getX();
            const float bx2 = (float) frac(fRes, 0.0, 0.0, 0.0).getX();
            const float by = (float) frac(0.0, 0.15, 0.0, 0.0).getY();
            g.setColour(kInk.withAlpha(0.55f));
            g.drawDashedLine({ bx1, by, bx2, by }, dashes, 2, 3.0f);
            g.setColour(kInk.withAlpha(0.9f));
            g.drawText("BP", juce::Rectangle<int>((int) bx1 - 30, 3, 60, 30),
                       juce::Justification::centred);
            g.drawText("LP", juce::Rectangle<int>((int) bx2 - 30, 3, 60, 30),
                       juce::Justification::centred);

            // P3: response-curve icons beside the labels (band-pass hump,
            // low-pass shelf with the resonance bump).
            juce::Path bp, lp;
            const float bx = bx1 - 65.0f, lx = bx2 + 65.0f, cy = 22.0f;
            bp.startNewSubPath(bx - 26.0f, cy + 14.0f);
            bp.cubicTo(bx - 10.0f, cy + 12.0f, bx - 12.0f, cy - 14.0f, bx, cy - 14.0f);
            bp.cubicTo(bx + 12.0f, cy - 14.0f, bx + 10.0f, cy + 12.0f, bx + 26.0f, cy + 14.0f);
            lp.startNewSubPath(lx - 26.0f, cy - 6.0f);
            lp.lineTo(lx - 4.0f, cy - 6.0f);
            lp.cubicTo(lx + 2.0f, cy - 14.0f, lx + 8.0f, cy - 14.0f, lx + 11.0f, cy - 6.0f);
            lp.cubicTo(lx + 14.0f, cy + 2.0f, lx + 17.0f, cy + 10.0f, lx + 24.0f, cy + 14.0f);
            g.strokePath(bp, juce::PathStrokeType(3.0f));
            g.strokePath(lp, juce::PathStrokeType(3.0f));
        }

        // "link" print under the center toggle.
        g.setColour(kInk);
        g.drawText("link", frac(0.44, 0.58, 0.12, 0.14), juce::Justification::centred);
    }

    std::unique_ptr<LabeledKnob> freqL, resL, dist, gain, freqR, resR;
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
// 1-2-3 program toggles, MOD L/R (filter CV amounts — the print puts them
// here, above the CV L/R jacks), BLEND, MASTER VOLUME, headphones print.
class EffectorSection : public Section
{
public:
    explicit EffectorSection(Apvts& s) : Section("DUAL EFFECTOR", Band::TopCentre)
    {
        cart = std::make_unique<ChoiceBox>(s, "fx.cart", "");
        progL = std::make_unique<Prog3Switch>(s, "fx.progL", "L");
        progR = std::make_unique<Prog3Switch>(s, "fx.progR", "R");
        x = std::make_unique<LabeledKnob>(s, "fx.x", "X", kKnobOrange);
        y = std::make_unique<LabeledKnob>(s, "fx.y", "Y", kKnobOrange);
        z = std::make_unique<LabeledKnob>(s, "fx.z", "Z", kKnobOrange);
        modL = std::make_unique<LabeledKnob>(s, "filt.modL", "MOD L", kKnobBlack);
        modR = std::make_unique<LabeledKnob>(s, "filt.modR", "MOD R", kKnobBlack);
        blend = std::make_unique<LabeledKnob>(s, "fx.blend", "", kKnobOrange);
        master = std::make_unique<LabeledKnob>(s, "master.vol", "", kKnobOrange);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 cart.get(), progL.get(), progR.get(), x.get(), y.get(), z.get(),
                 modL.get(), modR.get(), blend.get(), master.get() })
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
        place(*cart, 0.36, 0.26, 0.245, 0.34);
        place(*progR, 0.625, 0.24, 0.085, 0.52);
        // MOD L/R centred over the filter strip's CV L / CV R jacks
        // (kEffector and kFilter share x/w, so the fractions line up).
        place(*modL, 0.30, 0.755, 0.09, 0.235);
        place(*modR, 0.61, 0.755, 0.09, 0.235);
        place(*blend, 0.725, 0.34, 0.10, 0.42);
        place(*master, 0.80, 0.28, 0.14, 0.56);
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Cartridge bay: a slot frame around the inserted-cartridge selector,
        // flanked by the two 1-2-3 program toggles.
        const auto bay = frac(0.345, 0.18, 0.275, 0.50).toFloat();
        g.setColour(juce::Colour(0xff2b2b30));
        g.fillRoundedRectangle(bay, 12.0f);
        g.setColour(kInk);
        g.drawRoundedRectangle(bay, 12.0f, 4.0f);
        g.setColour(kCream.withAlpha(0.85f));
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.drawText("cartridge slot", bay.withTrimmedBottom(bay.getHeight() * 0.72f),
                   juce::Justification::centred);
        g.setColour(kCream.withAlpha(0.6f));
        g.setFont(juce::FontOptions(27.0f, juce::Font::bold));
        g.drawText("flip 1-2-3 to load", bay.withTrimmedTop(bay.getHeight() * 0.74f),
                   juce::Justification::centred);

        // BLEND / MASTER VOLUME captions above their knobs, like the print.
        g.setColour(kInk);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText("BLEND", frac(0.70, 0.25, 0.15, 0.08), juce::Justification::centred);
        g.drawFittedText("MASTER\nVOLUME", frac(0.795, 0.13, 0.15, 0.14),
                         juce::Justification::centred, 2);

        // Headphones out: glyph + jack + level print (inventory §1g). The
        // behavior (standalone monitor level) is an M9c decision; until then
        // this is silkscreen, not a control.
        const float hx = (float) frac(0.966, 0.0, 0.0, 0.0).getX();
        const float hy = (float) frac(0.0, 0.20, 0.0, 0.0).getY();
        juce::Path band;
        band.addCentredArc(hx, hy, 30.0f, 30.0f, 0.0f,
                           -juce::MathConstants<float>::halfPi * 1.1f,
                           juce::MathConstants<float>::halfPi * 1.1f, true);
        g.setColour(kInk);
        g.strokePath(band, juce::PathStrokeType(8.0f));
        g.fillRoundedRectangle(hx - 38.0f, hy - 8.0f, 15.0f, 32.0f, 5.0f);
        g.fillRoundedRectangle(hx + 23.0f, hy - 8.0f, 15.0f, 32.0f, 5.0f);
        const float jy = hy + 90.0f;
        g.drawEllipse(hx - 28.0f, jy - 28.0f, 56.0f, 56.0f, 6.0f);
        g.setColour(juce::Colour(0xff17171a));
        g.fillEllipse(hx - 20.0f, jy - 20.0f, 40.0f, 40.0f);
        const float ky = jy + 92.0f;
        g.setColour(kInk);
        g.fillEllipse(hx - 34.0f, ky - 34.0f, 68.0f, 68.0f);
        g.setColour(kKnobOrange);
        g.fillEllipse(hx - 27.0f, ky - 27.0f, 54.0f, 54.0f);
        g.setColour(kCream);
        g.drawLine(hx, ky - 24.0f, hx, ky - 6.0f, 5.0f);
    }

    std::unique_ptr<ChoiceBox> cart;
    std::unique_ptr<Prog3Switch> progL, progR;
    std::unique_ptr<LabeledKnob> x, y, z, modL, modR, blend, master;
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

        // P3: the wave knob sweeps triangle -> pulse; the print flanks the
        // caption with the two shapes.
        g.setColour(kInk);
        const auto tri = frac(0.077, 0.655, 0, 0).getPosition().toFloat();
        const auto pul = frac(0.253, 0.655, 0, 0).getPosition().toFloat();
        juce::Path p;
        p.startNewSubPath(tri.x - 10.0f, tri.y + 9.0f);
        p.lineTo(tri.x, tri.y - 9.0f);
        p.lineTo(tri.x + 10.0f, tri.y + 9.0f);
        p.startNewSubPath(pul.x - 10.0f, pul.y + 9.0f);
        p.lineTo(pul.x - 10.0f, pul.y - 9.0f);
        p.lineTo(pul.x + 10.0f, pul.y - 9.0f);
        p.lineTo(pul.x + 10.0f, pul.y + 9.0f);
        g.strokePath(p, juce::PathStrokeType(3.5f));
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
    void paintExtras(juce::Graphics& g) override
    {
        // Activity LEDs beside the stacked jacks (print: red dots flanking
        // the column; ours sit right of the labels' side).
        led(g, frac(0.68, 0.26, 0.0, 0.0).getPosition().toFloat(), 0.0f,
            kAccentRed, 13.0f);
        led(g, frac(0.68, 0.62, 0.0, 0.0).getPosition().toFloat(), 0.0f,
            kAccentRed, 13.0f);

        // P3: the print's curved routing arrows, offset knob -> out jack.
        g.setColour(kAccentRed.withAlpha(0.9f));
        printArrow(g, frac(0.240, 0.620, 0, 0).getPosition().toFloat(),
                   frac(0.306, 0.745, 0, 0).getPosition().toFloat(),
                   frac(0.389, 0.579, 0, 0).getPosition().toFloat(),
                   frac(0.435, 0.390, 0, 0).getPosition().toFloat(), 4.0f, 15.0f);
        printArrow(g, frac(0.765, 0.630, 0, 0).getPosition().toFloat(),
                   frac(0.737, 0.745, 0, 0).getPosition().toFloat(),
                   frac(0.670, 0.735, 0, 0).getPosition().toFloat(),
                   frac(0.608, 0.672, 0, 0).getPosition().toFloat(), 4.0f, 15.0f);
    }

    std::unique_ptr<LabeledKnob> xoff, yoff;
};

class SeqSection : public Section
{
public:
    // Title band painted by hand: text left + the striped voltage-ruler
    // print filling the rest, like the hardware.
    explicit SeqSection(Apvts& s) : Section("", Band::Bottom)
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

        // Bottom band: title at the left, voltage-ruler stripes to the right.
        const auto band = getLocalBounds().toFloat().reduced(4.0f)
                              .removeFromBottom((float) kBand).reduced(5.0f);
        g.setColour(kCream);
        g.setFont(juce::FontOptions((float) kBand * 0.62f, juce::Font::bold));
        g.drawText("5 STEP SEQ.  VOLTAGE", band.reduced(24.0f, 0.0f).toNearestInt(),
                   juce::Justification::centredLeft);
        for (float sx = band.getX() + 560.0f; sx < band.getRight() - 16.0f; sx += 17.0f)
            g.fillRect(sx, band.getCentreY() - band.getHeight() * 0.28f,
                       7.0f, band.getHeight() * 0.56f);
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

    // Medium skirted red knob like the print (was authored small).
    void resized() override { place(*gain, 0.36, 0.06, 0.46, 0.72); }

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

        // P3: microphone glyph under the LED, per print.
        const float mx = (float) frac(0.87, 0.0, 0, 0).getX();
        g.setColour(kInk);
        g.fillRoundedRectangle(mx - 14.0f, 118.0f, 28.0f, 48.0f, 14.0f);
        juce::Path u;
        u.addCentredArc(mx, 152.0f, 24.0f, 24.0f, 0.0f,
                        juce::MathConstants<float>::halfPi,
                        3.0f * juce::MathConstants<float>::halfPi, true);
        g.strokePath(u, juce::PathStrokeType(5.0f));
        g.drawLine(mx, 176.0f, mx, 200.0f, 5.0f);
        g.drawLine(mx - 16.0f, 200.0f, mx + 16.0f, 200.0f, 5.0f);
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

        // P3: the print's follower glyph — a red envelope traced over a
        // black signal burst — between the knobs and the jacks.
        const float gx = (float) frac(0.525, 0.0, 0, 0).getX();
        const float base = 145.0f;
        static constexpr float bars[11] = { 6.0f, 14.0f, 24.0f, 34.0f, 42.0f, 46.0f,
                                            42.0f, 34.0f, 24.0f, 14.0f, 6.0f };
        g.setColour(kInk);
        for (int i = 0; i < 11; ++i)
        {
            const float x = gx + 6.4f * ((float) i - 5.0f);
            g.drawLine(x, base, x, base - bars[i], 3.0f);
        }
        juce::Path env;
        env.startNewSubPath(gx - 35.0f, base);
        env.cubicTo(gx - 15.0f, base - 5.0f, gx - 17.0f, base - 77.0f, gx, base - 77.0f);
        env.cubicTo(gx + 17.0f, base - 77.0f, gx + 15.0f, base - 5.0f, gx + 35.0f, base);
        g.setColour(kAccentRed);
        g.strokePath(env, juce::PathStrokeType(4.0f));
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

        // Diagonal corner dots, per the print.
        for (int i = 0; i < 4; ++i)
        {
            const float ang = juce::MathConstants<float>::halfPi * ((float) i + 0.5f);
            const auto p = r.getCentre().getPointOnCircumference(
                r.getWidth() * 0.5f - 26.0f, ang);
            g.fillEllipse(p.x - 9.0f, p.y - 9.0f, 18.0f, 18.0f);
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
// Print look (M9b P2): square dark keycaps with a bevelled top face and a
// light LED notch at the top centre; the voice number prints ABOVE each pad.
class KeypadSection : public juce::Component
{
public:
    explicit KeypadSection(Apvts& s)
    {
        const char* ids[6] = { "d1", "d4", "d2", "d5", "d3", "d6" }; // row-major
        for (int i = 0; i < 6; ++i)
        {
            const auto paramId = juce::String(ids[i]) + ".gate";
            keys[i] = std::make_unique<KeycapButton>();
            if (auto* p = s.getParameter(paramId))
                keys[i]->setTooltip(tips::controlTooltip(paramId, p->getName(64)));
            addAndMakeVisible(*keys[i]);
            attach[i] = std::make_unique<Apvts::ButtonAttachment>(s, paramId, *keys[i]);
            numbers[i] = juce::String(ids[i]).substring(1);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(52.0f, juce::Font::bold));
        g.drawText("DRONE VOICES", getLocalBounds().removeFromTop(kTitleH),
                   juce::Justification::centred);
        g.setFont(juce::FontOptions(46.0f, juce::Font::bold));
        for (int i = 0; i < 6; ++i)
        {
            const auto cell = padRect(i);
            g.drawText(numbers[i],
                       cell.withY(cell.getY() - kNumberH).withHeight(kNumberH),
                       juce::Justification::centred);
        }
    }

    void resized() override
    {
        for (int i = 0; i < 6; ++i)
            keys[i]->setBounds(padRect(i));
    }

private:
    // Square keycap with the print's up-left bevelled face + LED notch.
    struct KeycapButton : juce::ToggleButton
    {
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override
        {
            auto r = getLocalBounds().toFloat();
            const float sq = juce::jmin(r.getWidth(), r.getHeight());
            const auto pad = juce::Rectangle<float>(sq, sq).withCentre(r.getCentre());
            g.setColour(juce::Colour(0xff0d0d10)); // shadow edge (bottom/right)
            g.fillRoundedRectangle(pad, sq * 0.08f);
            auto face = pad.reduced(sq * 0.09f);
            if (!down) // keycap top face rides up-left until pressed
                face.translate(-sq * 0.035f, -sq * 0.035f);
            g.setColour(highlighted ? juce::Colour(0xff35353b) : juce::Colour(0xff2a2a2f));
            g.fillRoundedRectangle(face, sq * 0.06f);
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawRoundedRectangle(face, sq * 0.06f, 2.5f);

            const bool on = getToggleState();
            const juce::Rectangle<float> led(face.getCentreX() - sq * 0.07f,
                                             face.getY() + sq * 0.03f,
                                             sq * 0.14f, sq * 0.16f);
            if (on)
            {
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.fillRoundedRectangle(led.expanded(8.0f), 8.0f);
            }
            g.setColour(on ? juce::Colours::white : juce::Colours::white.withAlpha(0.35f));
            g.fillRoundedRectangle(led, 4.0f);
        }
    };

    // 3 rows x 2 columns; number band above each pad. Tight column gap per
    // the print (pads nearly touch side to side, rows leave room for numbers).
    static constexpr int kTitleH = 64, kNumberH = 52, kPadSz = 168, kRowPitch = 232;
    juce::Rectangle<int> padRect(int i) const
    {
        const int col = i % 2, row = i / 2;
        const int x0 = (getWidth() - 2 * kPadSz - 30) / 2;
        return { x0 + col * (kPadSz + 30), kTitleH + kNumberH + row * kRowPitch,
                 kPadSz, kPadSz };
    }

    std::unique_ptr<KeycapButton> keys[6];
    std::unique_ptr<Apvts::ButtonAttachment> attach[6];
    juce::String numbers[6];
};

} // namespace solar
