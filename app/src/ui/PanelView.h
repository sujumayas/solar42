#pragma once

#include "engine/Rack.h"
#include "ui/CableLayer.h"
#include "ui/KeyboardSection.h"
#include "ui/PanelLayout.h"
#include "ui/PanelSections.h"

namespace solar {

// The full skeuomorphic panel (M5/M6): every section, the jack print layer
// and the cable layer stacked on top, plus the performance zone (physical
// stick, playable touch keyboard, DRONE VOICES keypad). Lives in the
// logical 4950 x 3200 space; the host editor scales it with one transform and
// drives zoom/pan through the callbacks below.
class PanelView : public juce::Component,
                  public PanelSurface,
                  private juce::Timer
{
public:
    static constexpr int kLogicalW = layout::kPanelW;
    static constexpr int kLogicalH = layout::kPanelH;

    std::function<void(juce::Point<float>)> onPan;          // screen-space delta
    std::function<void(juce::Rectangle<int>)> onZoomToRect; // logical panel rect
    std::function<void()> onOpenKeyboardSettings;           // editor opens the drawer
    std::function<void()> onCartReset; // effector reset/load button (P4)

    PanelView(Apvts& s, s42::Rack& rack, PatchBay& bay,
              KeyboardState& kbState, KbTouchState& kbTouch)
        : rack_(rack),
          drone1(s, "d1", "DRONE 1"), drone2(s, "d2", "DRONE 2"),
          drone4(s, "d4", "DRONE 4"), drone5(s, "d5", "DRONE 5"),
          drone3(s, "d3", "DRONE 3"), drone6(s, "d6", "DRONE 6"),
          vcoA(s, "vcoA", "VCO A"), vcoB(s, "vcoB", "VCO B"),
          envA(s, "envA", "envelope A"), envB(s, "envB", "envelope B"),
          filter(s), mixer(s), effector(s),
          lfoA(s, "lfoA", "LFO A"), lfoB(s, "lfoB", "LFO B"),
          joystick(s), seq(s), preamp(s), envFollower(s),
          joyPad(s, "joy.x", "joy.y"), keyboard(kbState, kbTouch), keypad(s),
          roomLight(s, "room.light", "ROOM LIGHT", kKnobBlack),
          flicker(s, "room.flicker", "50 Hz"),
          cables(bay)
    {
        setLookAndFeel(&lnf_);
        // Also the process default: raw g.setFont(FontOptions(...)) calls in
        // paint code resolve their typeface through the DEFAULT LookAndFeel,
        // not the component one — this is what puts ABC Solar on every label
        // (incl. desktop-space siblings like PresetBar / SettingsDrawer).
        juce::LookAndFeel::setDefaultLookAndFeel(&lnf_);
        keyboard.onOpenSettings = [this]
        {
            if (onOpenKeyboardSettings)
                onOpenKeyboardSettings();
        };
        effector.onCartReset = [this]
        {
            if (onCartReset)
                onCartReset();
        };
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &drone1, &drone2, &drone4, &drone5, &drone3, &drone6,
                 &vcoA, &vcoB, &envA, &envB, &filter, &mixer, &effector,
                 &lfoA, &lfoB, &joystick, &seq, &preamp, &envFollower,
                 &joyPad, &keyboard, &keypad, &roomLight, &flicker, &jacks, &cables })
            addAndMakeVisible(c);
        setSize(kLogicalW, kLogicalH);
        startTimerHz(30);
    }

    ~PanelView() override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        setLookAndFeel(nullptr);
    }

    void panByScreenDelta(juce::Point<float> d) override
    {
        if (onPan)
            onPan(d);
    }

    void zoomToPanelRect(juce::Rectangle<int> r) override
    {
        if (onZoomToRect)
            onZoomToRect(r);
    }

    // Background (header / performance-zone) drags pan, like sections.
    void mouseDown(const juce::MouseEvent& e) override
    {
        lastScreenPos_ = e.getScreenPosition().toFloat();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        panByScreenDelta(e.getScreenPosition().toFloat() - lastScreenPos_);
        lastScreenPos_ = e.getScreenPosition().toFloat();
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        zoomToPanelRect(getLocalBounds()); // reset to fit
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kCream);
        paintHeader(g);
        paintPerformanceZone(g);

        // Center column between the envelopes: "VOICE MIXER" badge over the
        // logo stand-in, matching the hardware print (badge above the logo).
        const juce::Rectangle<int> mid(layout::kEnvA.x + layout::kEnvA.w,
                                       layout::kEnvA.y,
                                       layout::kEnvB.x - layout::kEnvA.x - layout::kEnvA.w,
                                       layout::kEnvA.h);
        const auto badge = mid.reduced(26, 0).withY(mid.getY() + 24).withHeight(170).toFloat();
        g.setColour(kInk);
        g.fillRoundedRectangle(badge, 14.0f);
        g.setColour(kCream);
        g.setFont(juce::FontOptions(46.0f, juce::Font::bold));
        const auto badgeInt = badge.toNearestInt().reduced(8, 12);
        g.drawFittedText("VOICE", badgeInt.withHeight(badgeInt.getHeight() / 2),
                         juce::Justification::centred, 1);
        g.drawFittedText("MIXER", badgeInt.withTrimmedTop(badgeInt.getHeight() / 2),
                         juce::Justification::centred, 1);
        g.setColour(kInk);
        g.setFont(fonts::display(56.0f));
        g.drawFittedText("S42N", mid.withTrimmedTop(230).withTrimmedBottom(40),
                         juce::Justification::centred, 2);
    }

    void resized() override
    {
        auto place = [](juce::Component& c, const layout::Rect& r)
        {
            c.setBounds(r.x, r.y, r.w, r.h);
        };
        place(drone1, layout::kDrone1);
        place(drone2, layout::kDrone2);
        place(effector, layout::kEffector);
        place(drone4, layout::kDrone4);
        place(drone5, layout::kDrone5);
        place(filter, layout::kFilter);
        place(mixer, layout::kMixer);
        place(envA, layout::kEnvA);
        place(envB, layout::kEnvB);
        place(drone3, layout::kDrone3);
        place(vcoA, layout::kVcoA);
        place(vcoB, layout::kVcoB);
        place(drone6, layout::kDrone6);
        place(lfoA, layout::kLfoA);
        place(joystick, layout::kJoystick);
        place(seq, layout::kSeq);
        place(preamp, layout::kPreamp);
        place(envFollower, layout::kEnvFollower);
        place(lfoB, layout::kLfoB);
        place(joyPad, layout::kJoyPad);
        place(keyboard, layout::kKeyboard);
        place(keypad, layout::kKeypad);
        place(roomLight, layout::kRoomLight);
        place(flicker, layout::kFlicker);
        jacks.setBounds(getLocalBounds());
        cables.setBounds(getLocalBounds());
    }

private:
    void timerCallback() override
    {
        s42::TelemetryData t;
        if (!rack_.telemetry().read(t))
            return;
        ClassicDroneSection* classics[4] = { &drone1, &drone2, &drone4, &drone5 };
        for (int v = 0; v < 4; ++v)
            classics[v]->setTelemetry(t.droneGen[v], t.sensor[v]);
        drone3.setTelemetry(t.srapaCv[0]);
        drone6.setTelemetry(t.srapaCv[1]);
        lfoA.setTelemetry(t.lfoA);
        lfoB.setTelemetry(t.lfoB);
        seq.setTelemetry(t.seqStep, t.seqGate);
        preamp.setTelemetry(t.preampClip);
        envFollower.setTelemetry(t.follower, t.followerGate);
        keyboard.setTelemetry(t);
    }

    void paintHeader(juce::Graphics& g)
    {
        // Jack block first (top-left, like the render): the real EXT. AUDIO
        // jack (JackLayer) at (340, 110) with the print-only WET OUT pair
        // (side-panel connectors on hardware) to its right.
        g.setFont(juce::FontOptions(34.0f, juce::Font::bold));
        for (int i = 0; i < 2; ++i)
        {
            const juce::Point<float> c(560.0f + 150.0f * (float) i, 110.0f);
            g.setColour(juce::Colour(0xff3a3a40));
            g.fillEllipse(c.x - 33.0f, c.y - 33.0f, 66.0f, 66.0f);
            g.setColour(juce::Colours::black);
            g.fillEllipse(c.x - 20.0f, c.y - 20.0f, 40.0f, 40.0f);
            g.setColour(kInk);
            g.drawText(i == 0 ? "R" : "L", (int) c.x - 30, 30, 60, 40,
                       juce::Justification::centred);
        }
        g.setColour(kInk);
        g.drawText("WET OUT", 490, 165, 300, 40, juce::Justification::centred);
        // Bracket curling under the R / L pair into the caption, per print.
        const float by = 192.0f;
        g.drawLine(505.0f, by, 550.0f, by, 4.0f);
        g.drawLine(505.0f, by, 505.0f, by - 18.0f, 4.0f);
        g.drawLine(730.0f, by, 775.0f, by, 4.0f);
        g.drawLine(775.0f, by, 775.0f, by - 18.0f, 4.0f);
        g.drawText("POWER   12V DC", 4530, 130, 380, 40, juce::Justification::centredLeft);
        // P3: barrel polarity glyph (centre pin +) and the print's curled
        // bracket under the POWER caption.
        g.fillRoundedRectangle(4798.0f, 147.0f, 18.0f, 6.0f, 3.0f);
        g.drawLine(4816.0f, 150.0f, 4833.0f, 150.0f, 3.5f);
        {
            juce::Path c;
            c.addCentredArc(4846.0f, 150.0f, 13.0f, 13.0f, 0.0f,
                            juce::MathConstants<float>::halfPi + 0.7f,
                            juce::MathConstants<float>::halfPi - 0.7f
                                + juce::MathConstants<float>::twoPi,
                            true);
            g.strokePath(c, juce::PathStrokeType(3.5f));
        }
        g.fillEllipse(4841.0f, 145.0f, 10.0f, 10.0f);
        g.drawLine(4851.0f, 146.0f, 4864.0f, 138.0f, 3.5f);
        g.drawLine(4864.0f, 136.0f, 4880.0f, 136.0f, 3.5f);
        g.drawLine(4872.0f, 128.0f, 4872.0f, 144.0f, 3.5f);
        {
            juce::Path b;
            b.startNewSubPath(4525.0f, 170.0f);
            b.quadraticTo(4525.0f, 187.0f, 4543.0f, 187.0f);
            b.lineTo(4897.0f, 187.0f);
            b.quadraticTo(4915.0f, 187.0f, 4915.0f, 170.0f);
            g.strokePath(b, juce::PathStrokeType(4.0f));
        }

        // Digital-only corner (ROOM LIGHT + 50 Hz have no silk-screened home
        // on the hardware): a dashed outline marks them as ours.
        {
            juce::Path box, dashed;
            box.addRoundedRectangle(4005.0f, 66.0f, 512.0f, 276.0f, 24.0f);
            const float dl[2] = { 14.0f, 12.0f };
            juce::PathStrokeType(3.5f).createDashedStroke(dashed, box, dl, 2);
            g.setColour(kInk.withAlpha(0.45f));
            g.fillPath(dashed);
        }

        // Wordmark below the jack block (render: y ~250..500, left of the
        // effector): black SOLAR, red 42, black superscript N — the print
        // lockup. Original branding replaces the Elta trade dress before
        // any public build (08-implementation-plan.md §Risks).
        const juce::Font wordmark = fonts::display(200.0f);
        juce::AttributedString ws;
        ws.append("SOLAR ", wordmark, kInk);
        ws.append("42", wordmark, kAccentRed);
        ws.setJustification(juce::Justification::bottomLeft);
        ws.draw(g, juce::Rectangle<float>(60.0f, 230.0f, 1500.0f, 250.0f));
        juce::GlyphArrangement ga;
        ga.addLineOfText(wordmark, "SOLAR 42", 0.0f, 0.0f);
        const float nX = 60.0f + ga.getBoundingBox(0, -1, true).getRight() + 12.0f;
        g.setColour(kInk);
        g.setFont(fonts::display(120.0f));
        g.drawText("N", (int) nX, 292, 140, 95, juce::Justification::centredLeft);

        // The print's diacritics (dot-A, macron-E) + a touch of tracking.
        g.setColour(kInk);
        juce::Font ambient(juce::FontOptions(96.0f, juce::Font::bold));
        ambient.setExtraKerningFactor(0.05f);
        g.setFont(ambient);
        g.drawFittedText(juce::String(juce::CharPointer_UTF8("\xC8\xA6MBI\xC4\x92NT\nMACHINE")),
                         juce::Rectangle<int>(2950, 70, 1000, 230),
                         juce::Justification::centredRight, 2);
    }

    void paintPerformanceZone(juce::Graphics& g)
    {
        g.setColour(kInk);
        g.setFont(juce::FontOptions(52.0f, juce::Font::bold));
        g.drawText("MICROTONAL  DRONE  SYNTHESIZER",
                   layout::kKeyboard.x, layout::kBottom.y + 20, 1400, 60,
                   juce::Justification::centredLeft);
        // Keyboard CV strip: the hardware wears these on the keyboard block
        // itself (the render art hides them); a thin outlined sub-panel
        // groups them as part of the keyboard. Wide enough for the
        // digital-only MIDI CLK jack at the strip's end (M9c P4).
        g.setColour(kInk.withAlpha(0.85f));
        g.drawRoundedRectangle(3130.0f, 2158.0f, 1205.0f, 222.0f, 16.0f, 4.0f);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText("KEYBOARD", 3150, 2164, 240, 34, juce::Justification::centredLeft);

        // СОЛАР 42N print at the keyboard block's right shoulder (per print).
        const juce::Font cy(juce::FontOptions(52.0f, juce::Font::bold));
        juce::AttributedString solap;
        solap.append(juce::String(juce::CharPointer_UTF8(
                         "\xD0\xA1\xD0\x9E\xD0\x9B\xD0\x90\xD0\xA0 ")), cy, kInk);
        solap.append("42N", cy, kAccentRed);
        solap.setJustification(juce::Justification::centredRight);
        solap.draw(g, juce::Rectangle<float>(2450.0f, (float) layout::kBottom.y + 20.0f,
                                             600.0f, 60.0f));
        // The playable KeyboardSection component owns the keyboard zone.
    }

    SolarLookAndFeel lnf_;
    s42::Rack& rack_;

    ClassicDroneSection drone1, drone2, drone4, drone5;
    PapaSrapaSection drone3, drone6;
    VcoSection vcoA, vcoB;
    EnvelopeSection envA, envB;
    FilterSection filter;
    MixerSection mixer;
    EffectorSection effector;
    LfoSection lfoA, lfoB;
    JoystickSection joystick;
    SeqSection seq;
    PreampSection preamp;
    EnvFollowerSection envFollower;
    JoyPad joyPad;
    KeyboardSection keyboard;
    KeypadSection keypad;
    LabeledKnob roomLight;
    SlideSwitch flicker;
    JackLayer jacks;
    CableLayer cables;

    juce::Point<float> lastScreenPos_;
};

} // namespace solar
