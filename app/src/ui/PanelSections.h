#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/SolarLookAndFeel.h"

// Panel UI phase 1 widget kit + one component per silk-screened section
// (top half + mod strip). Sections lay out in LOGICAL panel units — the
// PanelEditor scales the whole panel with one AffineTransform, so fonts and
// strokes stay proportional (and M5's zoom comes for free). Jacks, LEDs and
// telemetry glow join in M5 with the measured per-control PanelLayout.
namespace solar {

using Apvts = juce::AudioProcessorValueTreeState;

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
        addAndMakeVisible(label);
        attach = std::make_unique<Apvts::SliderAttachment>(s, paramId, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto text = r.removeFromBottom(juce::jmax(24, r.getHeight() / 5));
        label.setFont(juce::FontOptions((float) text.getHeight() * 0.72f));
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
    }

    void resized() override { button.setBounds(getLocalBounds()); }

    juce::ToggleButton button;
    std::unique_ptr<Apvts::ButtonAttachment> attach;
};

// Round push button with an LED dot (MUTE / MOD / HOLD / GATE).
struct PushButton : juce::Component
{
    PushButton(Apvts& s, const juce::String& paramId, const juce::String& text,
               juce::Colour led)
    {
        button.setButtonText(text);
        button.setColour(juce::TextButton::buttonOnColourId, led);
        addAndMakeVisible(button);
        attach = std::make_unique<Apvts::ButtonAttachment>(s, paramId, button);
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
            box.addItemList(p->choices, 1);
        addAndMakeVisible(box);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        attach = std::make_unique<Apvts::ComboBoxAttachment>(s, paramId, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto text = r.removeFromBottom(juce::jmax(24, r.getHeight() / 3));
        label.setFont(juce::FontOptions((float) text.getHeight() * 0.7f));
        label.setBounds(text);
        box.setBounds(r.reduced(2, juce::jmax(0, (r.getHeight() - 64) / 2)));
    }

    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<Apvts::ComboBoxAttachment> attach;
};

// ---------------------------------------------------------------- section base

class Section : public juce::Component
{
public:
    explicit Section(juce::String title) : title_(std::move(title)) {}

    static constexpr int kBand = 64; // logical title-band height

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(kSectionBg);
        g.fillRoundedRectangle(r, 18.0f);
        g.setColour(kInk);
        g.drawRoundedRectangle(r, 18.0f, 5.0f);

        auto band = r.removeFromTop((float) kBand);
        g.fillRoundedRectangle(band.withTrimmedBottom(-10.0f).reduced(5.0f), 14.0f);
        g.setColour(kCream);
        g.setFont(juce::FontOptions((float) kBand * 0.62f, juce::Font::bold));
        g.drawText(title_, band.reduced(24.0f, 0.0f), juce::Justification::centredLeft);

        paintExtras(g);
    }

protected:
    virtual void paintExtras(juce::Graphics&) {}

    juce::Rectangle<int> content() const
    {
        return getLocalBounds().reduced(18).withTrimmedTop(kBand + 6);
    }

    // Evenly split a strip into n columns, return column i (0-based).
    static juce::Rectangle<int> col(juce::Rectangle<int> r, int n, int i, int span = 1)
    {
        const int w = r.getWidth() / n;
        return { r.getX() + i * w, r.getY(), w * span, r.getHeight() };
    }

private:
    juce::String title_;
};

// ---------------------------------------------------------------- voice sections

// Classic drone voice (DRONE 1/2/4/5): 5 gen columns MOD/TUNE/MUTE, LED bar +
// VOLT, GATE/HOLD/ATT/RLS row, photo-sensor window.
class ClassicDroneSection : public Section
{
public:
    ClassicDroneSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title)
    {
        for (int g = 0; g < 5; ++g)
        {
            const auto base = id + ".gen" + juce::String(g + 1);
            mod[g] = std::make_unique<PushButton>(s, base + ".mod", "", kAccentRed);
            tune[g] = std::make_unique<LabeledKnob>(s, base + ".tune", juce::String(g + 1), kKnobBlack);
            mute[g] = std::make_unique<PushButton>(s, base + ".mute", "", kKnobGrey);
            addAndMakeVisible(*mod[g]);
            addAndMakeVisible(*tune[g]);
            addAndMakeVisible(*mute[g]);
        }
        volt = std::make_unique<LabeledKnob>(s, id + ".volt", "VOLT", kKnobBlue);
        gate = std::make_unique<PushButton>(s, id + ".gate", "GATE", kKnobGreen);
        hold = std::make_unique<PushButton>(s, id + ".hold", "HOLD", juce::Colour(0xffe8c33a));
        att = std::make_unique<LabeledKnob>(s, id + ".att", "ATT", kKnobBlack);
        rls = std::make_unique<LabeledKnob>(s, id + ".rls", "RLS", kKnobBlack);
        for (juce::Component* c : { (juce::Component*) volt.get(), (juce::Component*) gate.get(),
                                    (juce::Component*) hold.get(), (juce::Component*) att.get(),
                                    (juce::Component*) rls.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        auto r = content();
        auto bottom = r.removeFromBottom(r.getHeight() * 2 / 7);
        auto right = r.removeFromRight(r.getWidth() / 4);
        gutter_ = r.removeFromLeft(76); // MOD / TUNE / MUTE row labels

        for (int g = 0; g < 5; ++g)
        {
            auto c = col(r, 5, g).reduced(4);
            mod[g]->setBounds(c.removeFromTop(c.getHeight() / 4));
            mute[g]->setBounds(c.removeFromBottom(c.getHeight() / 3));
            tune[g]->setBounds(c);
        }
        volt->setBounds(right.withTrimmedTop(right.getHeight() / 3).reduced(6));

        bottom.removeFromRight(bottom.getWidth() / 4); // sensor window (painted)
        sensor_ = getLocalBounds().toFloat().removeFromBottom((float) bottom.getHeight() + 20.0f)
                      .removeFromRight((float) bottom.getWidth() / 3.0f);
        gate->setBounds(col(bottom, 4, 0).reduced(6));
        hold->setBounds(col(bottom, 4, 1).reduced(6));
        att->setBounds(col(bottom, 4, 2).reduced(4));
        rls->setBounds(col(bottom, 4, 3).reduced(4));
    }

private:
    void paintExtras(juce::Graphics& g) override
    {
        // Photo-sensor window (glows with telemetry in M5).
        const float d = juce::jmin(sensor_.getWidth(), sensor_.getHeight()) * 0.8f;
        auto w = juce::Rectangle<float>(d, d).withCentre(sensor_.getCentre());
        g.setColour(juce::Colours::white);
        g.fillEllipse(w);
        g.setColour(kInk);
        g.drawEllipse(w, 4.0f);

        // Hardware row markers on the generator grid.
        g.setColour(kInk.withAlpha(0.8f));
        g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
        auto gut = gutter_.toFloat();
        g.drawText("MOD", gut.removeFromTop(gut.getHeight() / 4), juce::Justification::centred);
        auto muteRow = gut.removeFromBottom(gut.getHeight() / 3);
        g.drawText("MUTE", muteRow, juce::Justification::centred);
        g.drawText("TUNE", gut, juce::Justification::centred);
    }

    std::unique_ptr<PushButton> mod[5], mute[5], gate, hold;
    std::unique_ptr<LabeledKnob> tune[5], volt, att, rls;
    juce::Rectangle<float> sensor_;
    juce::Rectangle<int> gutter_;
};

// Papa Srapa voice (DRONE 3/6).
class PapaSrapaSection : public Section
{
public:
    PapaSrapaSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title)
    {
        rate = std::make_unique<LabeledKnob>(s, id + ".rate", "rate", kKnobBlue);
        fmamt = std::make_unique<LabeledKnob>(s, id + ".fmamt", "mod", kKnobBlue);
        divider = std::make_unique<ChoiceBox>(s, id + ".divider", "divider");
        pitch = std::make_unique<LabeledKnob>(s, id + ".pitch", "PITCH", kKnobBlue);
        noise = std::make_unique<LabeledKnob>(s, id + ".noise", "NOISE", kKnobBlue);
        fm = std::make_unique<SlideSwitch>(s, id + ".fm", "fm");
        am = std::make_unique<SlideSwitch>(s, id + ".am", "am");
        x10 = std::make_unique<SlideSwitch>(s, id + ".x10", "x10");
        hi = std::make_unique<SlideSwitch>(s, id + ".hi", "hi");
        gate = std::make_unique<PushButton>(s, id + ".gate", "GATE", kKnobGreen);
        hold = std::make_unique<PushButton>(s, id + ".hold", "HOLD", juce::Colour(0xffe8c33a));
        att = std::make_unique<LabeledKnob>(s, id + ".att", "ATT", kKnobBlue);
        rls = std::make_unique<LabeledKnob>(s, id + ".rls", "RLS", kKnobBlue);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 rate.get(), fmamt.get(), divider.get(), pitch.get(), noise.get(),
                 fm.get(), am.get(), x10.get(), hi.get(),
                 gate.get(), hold.get(), att.get(), rls.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        auto r = content();
        auto switches = r.removeFromTop(r.getHeight() / 5);
        auto bottom = r.removeFromBottom(r.getHeight() / 3);

        x10->setBounds(col(switches, 4, 0).reduced(8, 2));
        fm->setBounds(col(switches, 4, 1).reduced(8, 2));
        am->setBounds(col(switches, 4, 2).reduced(8, 2));
        hi->setBounds(col(switches, 4, 3).reduced(8, 2));

        rate->setBounds(col(r, 5, 0).reduced(2));
        fmamt->setBounds(col(r, 5, 1).reduced(2));
        divider->setBounds(col(r, 5, 2).reduced(2));
        pitch->setBounds(col(r, 5, 3).reduced(2));
        noise->setBounds(col(r, 5, 4).reduced(2));

        gate->setBounds(col(bottom, 4, 0).reduced(6));
        hold->setBounds(col(bottom, 4, 1).reduced(6));
        att->setBounds(col(bottom, 4, 2).reduced(2));
        rls->setBounds(col(bottom, 4, 3).reduced(2));
    }

private:
    std::unique_ptr<LabeledKnob> rate, fmamt, pitch, noise, att, rls;
    std::unique_ptr<ChoiceBox> divider;
    std::unique_ptr<SlideSwitch> fm, am, x10, hi;
    std::unique_ptr<PushButton> gate, hold;
};

// VCO A/B.
class VcoSection : public Section
{
public:
    VcoSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title)
    {
        cvamt = std::make_unique<LabeledKnob>(s, id + ".cvamt", "cv amt", kKnobGreen);
        tune = std::make_unique<LabeledKnob>(s, id + ".tune", "tune", kKnobGreen);
        wave = std::make_unique<LabeledKnob>(s, id + ".wave", "WAVEFORM", kKnobGreen);
        pwm = std::make_unique<LabeledKnob>(s, id + ".pwm", "pwm", kKnobGreen);
        pw = std::make_unique<LabeledKnob>(s, id + ".pw", "pw", kKnobGreen);
        oct = std::make_unique<SlideSwitch>(s, id + ".oct", "oct+3");
        sub = std::make_unique<SlideSwitch>(s, id + ".sub", "-1 sub");
        exp = std::make_unique<SlideSwitch>(s, id + ".exp", "exp");
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 cvamt.get(), tune.get(), wave.get(), pwm.get(), pw.get(),
                 oct.get(), sub.get(), exp.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        auto r = content();
        auto left = r.removeFromLeft(r.getWidth() / 3);
        auto lTop = left.removeFromTop(left.getHeight() / 2);
        cvamt->setBounds(lTop.reduced(4));
        exp->setBounds(left.removeFromTop(left.getHeight() / 3).reduced(8, 4));
        pwm->setBounds(left.reduced(4));

        auto right = r.removeFromRight(r.getWidth() / 2);
        auto rTop = right.removeFromTop(right.getHeight() / 2);
        tune->setBounds(rTop.reduced(4));
        pw->setBounds(right.reduced(4));

        auto sw = r.removeFromTop(r.getHeight() / 4);
        oct->setBounds(col(sw, 2, 0).reduced(6, 2));
        sub->setBounds(col(sw, 2, 1).reduced(6, 2));
        wave->setBounds(r.reduced(2));
    }

private:
    std::unique_ptr<LabeledKnob> cvamt, tune, wave, pwm, pw;
    std::unique_ptr<SlideSwitch> oct, sub, exp;
};

// Envelope A/B.
class EnvelopeSection : public Section
{
public:
    EnvelopeSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title)
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
        auto r = content();
        auto side = r.removeFromLeft(r.getWidth() / 6);
        hold->setBounds(side.removeFromTop(side.getHeight() / 2).reduced(4));
        loop->setBounds(side.reduced(4));
        for (int i = 0; i < 4; ++i)
            adsr[i]->setBounds(col(r, 4, i).reduced(4));
    }

private:
    std::unique_ptr<LabeledKnob> adsr[4];
    std::unique_ptr<PushButton> hold, loop;
};

// Dual Polivoks filter strip + shared double distortion.
class FilterSection : public Section
{
public:
    explicit FilterSection(Apvts& s) : Section("FILTER  L | R")
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
        auto r = content();
        int i = 0;
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 freqL.get(), resL.get(), bpL.get(), modL.get(), dist.get(), link.get(),
                 gain.get(), modR.get(), bpR.get(), freqR.get(), resR.get() })
        {
            auto cell = col(r, 11, i++).reduced(4);
            if (dynamic_cast<SlideSwitch*>(c) != nullptr)
                cell = cell.reduced(6, cell.getHeight() / 5);
            c->setBounds(cell);
        }
    }

private:
    std::unique_ptr<LabeledKnob> freqL, resL, modL, dist, gain, modR, freqR, resR;
    std::unique_ptr<SlideSwitch> bpL, link, bpR;
};

// 10-channel voice mixer: PAN over VOL per channel; PAN is the filter routing.
class MixerSection : public Section
{
public:
    explicit MixerSection(Apvts& s) : Section("VOICE MIXER")
    {
        const char* ids[] = { "d1", "d2", "d3", "ext", "vcoA", "vcoB", "pre", "d4", "d5", "d6" };
        const char* names[] = { "DRONE 1", "DRONE 2", "DRONE 3", "EXT", "VCO A",
                                "VCO B", "PREAMP", "DRONE 4", "DRONE 5", "DRONE 6" };
        for (int c = 0; c < 10; ++c)
        {
            const auto base = "mix." + juce::String(ids[c]);
            pan[c] = std::make_unique<LabeledKnob>(s, base + ".pan", names[c], kKnobBlack);
            vol[c] = std::make_unique<LabeledKnob>(s, base + ".vol", "VOL", kKnobGrey);
            addAndMakeVisible(*pan[c]);
            addAndMakeVisible(*vol[c]);
        }
    }

    void resized() override
    {
        auto r = content();
        for (int c = 0; c < 10; ++c)
        {
            auto cell = col(r, 10, c).reduced(3);
            pan[c]->setBounds(cell.removeFromTop(cell.getHeight() / 2));
            vol[c]->setBounds(cell);
        }
    }

private:
    std::unique_ptr<LabeledKnob> pan[10], vol[10];
};

// DUAL EFFECTOR: cartridge slot, per-channel 1-2-3 program toggles, shared
// X/Y/Z + BLEND, MASTER VOLUME. Room light rides along until the M5 layout
// pass (it is digital-only and has no silk-screened home yet).
class EffectorSection : public Section
{
public:
    explicit EffectorSection(Apvts& s) : Section("DUAL EFFECTOR")
    {
        cart = std::make_unique<ChoiceBox>(s, "fx.cart", "CARTRIDGE");
        progL = std::make_unique<ChoiceBox>(s, "fx.progL", "1-2-3 L");
        progR = std::make_unique<ChoiceBox>(s, "fx.progR", "1-2-3 R");
        x = std::make_unique<LabeledKnob>(s, "fx.x", "X", kKnobOrange);
        y = std::make_unique<LabeledKnob>(s, "fx.y", "Y", kKnobOrange);
        z = std::make_unique<LabeledKnob>(s, "fx.z", "Z", kKnobOrange);
        blend = std::make_unique<LabeledKnob>(s, "fx.blend", "BLEND", kKnobOrange);
        master = std::make_unique<LabeledKnob>(s, "master.vol", "MASTER VOLUME", kKnobOrange);
        room = std::make_unique<LabeledKnob>(s, "room.light", "ROOM LIGHT", kKnobBlack);
        flicker = std::make_unique<SlideSwitch>(s, "room.flicker", "50 Hz");
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 cart.get(), progL.get(), progR.get(), x.get(), y.get(), z.get(),
                 blend.get(), master.get(), room.get(), flicker.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        auto r = content();
        auto top = r.removeFromTop(r.getHeight() / 3);
        cart->setBounds(col(top, 3, 0).reduced(4));
        progL->setBounds(col(top, 3, 1).reduced(4));
        progR->setBounds(col(top, 3, 2).reduced(4));
        int i = 0;
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 x.get(), y.get(), z.get(), blend.get(), master.get(), room.get() })
            c->setBounds(col(r, 7, i++).reduced(3));
        flicker->setBounds(col(r, 7, 6).reduced(4, r.getHeight() / 4));
    }

private:
    std::unique_ptr<ChoiceBox> cart, progL, progR;
    std::unique_ptr<LabeledKnob> x, y, z, blend, master, room;
    std::unique_ptr<SlideSwitch> flicker;
};

// ---------------------------------------------------------------- mod strip

class LfoSection : public Section
{
public:
    LfoSection(Apvts& s, const juce::String& id, const juce::String& title)
        : Section(title)
    {
        wave = std::make_unique<LabeledKnob>(s, id + ".wave", "wave", kKnobRed);
        rate = std::make_unique<LabeledKnob>(s, id + ".rate", "rate", kKnobRed);
        range = std::make_unique<ChoiceBox>(s, id + ".range", "");
        addAndMakeVisible(*wave);
        addAndMakeVisible(*rate);
        addAndMakeVisible(*range);
    }

    void resized() override
    {
        auto r = content();
        wave->setBounds(col(r, 3, 0).reduced(2));
        range->setBounds(col(r, 3, 1).reduced(4, r.getHeight() / 6));
        rate->setBounds(col(r, 3, 2).reduced(2));
    }

private:
    std::unique_ptr<LabeledKnob> wave, rate;
    std::unique_ptr<ChoiceBox> range;
};

class JoystickSection : public Section
{
public:
    explicit JoystickSection(Apvts& s) : Section("JOYSTICK")
    {
        x = std::make_unique<LabeledKnob>(s, "joy.x", "X", kKnobRed);
        y = std::make_unique<LabeledKnob>(s, "joy.y", "Y", kKnobRed);
        xoff = std::make_unique<LabeledKnob>(s, "joy.xoff", "offset X", kKnobRed);
        yoff = std::make_unique<LabeledKnob>(s, "joy.yoff", "offset Y", kKnobRed);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 x.get(), y.get(), xoff.get(), yoff.get() })
            addAndMakeVisible(c);
    }

    void resized() override
    {
        auto r = content();
        xoff->setBounds(col(r, 4, 0).reduced(2));
        x->setBounds(col(r, 4, 1).reduced(2));
        y->setBounds(col(r, 4, 2).reduced(2));
        yoff->setBounds(col(r, 4, 3).reduced(2));
    }

private:
    std::unique_ptr<LabeledKnob> x, y, xoff, yoff;
};

class SeqSection : public Section
{
public:
    explicit SeqSection(Apvts& s) : Section("5 STEP SEQ. VOLTAGE")
    {
        pulser = std::make_unique<LabeledKnob>(s, "seq.pulser", "pulser", kKnobRed);
        stages = std::make_unique<ChoiceBox>(s, "seq.stages", "stages");
        addAndMakeVisible(*pulser);
        addAndMakeVisible(*stages);
        for (int i = 0; i < 5; ++i)
        {
            const auto n = juce::String(i + 1);
            step[i] = std::make_unique<LabeledKnob>(s, "seq.step" + n, "step " + n, kKnobRed);
            gate[i] = std::make_unique<SlideSwitch>(s, "seq.gate" + n, "gate");
            addAndMakeVisible(*step[i]);
            addAndMakeVisible(*gate[i]);
        }
    }

    void resized() override
    {
        auto r = content();
        pulser->setBounds(col(r, 7, 0).reduced(2));
        stages->setBounds(col(r, 7, 1).reduced(6, r.getHeight() / 6));
        for (int i = 0; i < 5; ++i)
        {
            auto cell = col(r, 7, 2 + i).reduced(2);
            gate[i]->setBounds(cell.removeFromRight(cell.getWidth() / 3)
                                   .reduced(0, cell.getHeight() / 4));
            step[i]->setBounds(cell);
        }
    }

private:
    std::unique_ptr<LabeledKnob> pulser, step[5];
    std::unique_ptr<ChoiceBox> stages;
    std::unique_ptr<SlideSwitch> gate[5];
};

class PreampSection : public Section
{
public:
    explicit PreampSection(Apvts& s) : Section("PREAMP")
    {
        gain = std::make_unique<LabeledKnob>(s, "pre.gain", "gain", kKnobRed);
        addAndMakeVisible(*gain);
    }

    void resized() override { gain->setBounds(content().reduced(6)); }

private:
    std::unique_ptr<LabeledKnob> gain;
};

class EnvFollowerSection : public Section
{
public:
    explicit EnvFollowerSection(Apvts& s) : Section("ENVELOPE FOLLOWER")
    {
        att = std::make_unique<LabeledKnob>(s, "envf.att", "attack", kKnobRed);
        rel = std::make_unique<LabeledKnob>(s, "envf.rel", "release", kKnobRed);
        addAndMakeVisible(*att);
        addAndMakeVisible(*rel);
    }

    void resized() override
    {
        auto r = content();
        att->setBounds(col(r, 2, 0).reduced(4));
        rel->setBounds(col(r, 2, 1).reduced(4));
    }

private:
    std::unique_ptr<LabeledKnob> att, rel;
};

} // namespace solar
