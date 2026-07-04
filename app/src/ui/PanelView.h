#pragma once

#include "engine/Rack.h"
#include "ui/CableLayer.h"
#include "ui/PanelLayout.h"
#include "ui/PanelSections.h"

namespace solar {

// The full skeuomorphic panel (M5): every section, the jack print layer and
// the cable layer stacked on top, plus the performance zone (physical stick,
// keyboard placeholder print until M6, DRONE VOICES keypad). Lives in the
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

    PanelView(Apvts& s, s42::Rack& rack, PatchBay& bay)
        : rack_(rack),
          drone1(s, "d1", "DRONE 1"), drone2(s, "d2", "DRONE 2"),
          drone4(s, "d4", "DRONE 4"), drone5(s, "d5", "DRONE 5"),
          drone3(s, "d3", "DRONE 3"), drone6(s, "d6", "DRONE 6"),
          vcoA(s, "vcoA", "VCO A"), vcoB(s, "vcoB", "VCO B"),
          envA(s, "envA", "envelope A"), envB(s, "envB", "envelope B"),
          filter(s), mixer(s), effector(s),
          lfoA(s, "lfoA", "LFO A"), lfoB(s, "lfoB", "LFO B"),
          joystick(s), seq(s), preamp(s), envFollower(s),
          joyPad(s, "joy.x", "joy.y"), keypad(s),
          roomLight(s, "room.light", "ROOM LIGHT", kKnobBlack),
          flicker(s, "room.flicker", "50 Hz"),
          cables(bay)
    {
        setLookAndFeel(&lnf_);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &drone1, &drone2, &drone4, &drone5, &drone3, &drone6,
                 &vcoA, &vcoB, &envA, &envB, &filter, &mixer, &effector,
                 &lfoA, &lfoB, &joystick, &seq, &preamp, &envFollower,
                 &joyPad, &keypad, &roomLight, &flicker, &jacks, &cables })
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

        // Center badge between the envelopes (the render's logo spot).
        g.setColour(kInk);
        g.setFont(juce::FontOptions(56.0f, juce::Font::bold));
        g.drawFittedText("S42N",
                         juce::Rectangle<int>(layout::kEnvA.x + layout::kEnvA.w,
                                              layout::kEnvA.y + 60,
                                              layout::kEnvB.x - layout::kEnvA.x - layout::kEnvA.w,
                                              layout::kEnvA.h - 120),
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

        // Touch keyboard placeholder print — the playable surface lands in M6.
        const auto kb = juce::Rectangle<int>(layout::kKeyboard.x, layout::kKeyboard.y,
                                             layout::kKeyboard.w, layout::kKeyboard.h);
        g.setColour(juce::Colour(0xff17171a));
        g.fillRoundedRectangle(kb.toFloat(), 24.0f);
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.9f));
        for (int p = 0; p < 12; ++p) // 12 touch plates
        {
            const float w = (float) kb.getWidth() / 13.5f;
            const float x = (float) kb.getX() + w * 0.5f + w * (float) p;
            const float tall = (p % 2 == 0) ? 0.62f : 0.78f;
            g.drawRoundedRectangle(x, (float) kb.getY() + (float) kb.getHeight() * (1.0f - tall) * 0.5f,
                                   w * 0.72f, (float) kb.getHeight() * tall, 14.0f, 5.0f);
        }
        g.setFont(juce::FontOptions(40.0f, juce::Font::bold));
        g.drawText("TOUCH KEYBOARD - M6", kb.reduced(0, 8),
                   juce::Justification::centredBottom);
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
    KeypadSection keypad;
    LabeledKnob roomLight;
    SlideSwitch flicker;
    JackLayer jacks;
    CableLayer cables;

    juce::Point<float> lastScreenPos_;
};

} // namespace solar
