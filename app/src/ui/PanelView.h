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
        keyboard.onOpenSettings = [this]
        {
            if (onOpenKeyboardSettings)
                onOpenKeyboardSettings();
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

    ~PanelView() override { setLookAndFeel(nullptr); }

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
        g.setFont(juce::FontOptions(56.0f, juce::Font::bold));
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
        g.drawText("POWER   12V DC", 4530, 130, 380, 40, juce::Justification::centredLeft);

        // Wordmark below the jack block (render: y ~250..500, left of the
        // effector). Original branding replaces the Elta trade dress before
        // any public build (08-implementation-plan.md §Risks).
        const juce::Font wordmark(juce::FontOptions(200.0f, juce::Font::bold));
        juce::AttributedString ws;
        ws.append("SOLAR 42", wordmark, kInk);
        ws.append(" N", wordmark, kAccentRed);
        ws.setJustification(juce::Justification::bottomLeft);
        ws.draw(g, juce::Rectangle<float>(60.0f, 230.0f, 1500.0f, 250.0f));

        g.setColour(kInk);
        g.setFont(juce::FontOptions(96.0f, juce::Font::bold));
        g.drawFittedText("AMBIENT\nMACHINE",
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
