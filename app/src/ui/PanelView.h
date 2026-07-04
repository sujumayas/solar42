#pragma once

#include "ui/PanelLayout.h"
#include "ui/PanelSections.h"

namespace solar {

// The skeuomorphic panel, phase 1: every top-half + mod-strip section at its
// measured position in the logical 4950-wide space, drawn panel-style. The
// host editor scales this with one AffineTransform (fonts and strokes scale
// along — M5's zoom reuses exactly this). Height covers the panel down to the
// mod strip; the performance zone (keyboard/keypad, M6) is not part of it.
class PanelView : public juce::Component
{
public:
    static constexpr int kLogicalW = layout::kPanelW;
    static constexpr int kLogicalH = 2160; // header .. mod strip

    explicit PanelView(Apvts& s)
        : drone1(s, "d1", "DRONE 1"), drone2(s, "d2", "DRONE 2"),
          drone4(s, "d4", "DRONE 4"), drone5(s, "d5", "DRONE 5"),
          drone3(s, "d3", "DRONE 3"), drone6(s, "d6", "DRONE 6"),
          vcoA(s, "vcoA", "VCO A"), vcoB(s, "vcoB", "VCO B"),
          envA(s, "envA", "envelope A"), envB(s, "envB", "envelope B"),
          filter(s), mixer(s), effector(s),
          lfoA(s, "lfoA", "LFO A"), lfoB(s, "lfoB", "LFO B"),
          joystick(s), seq(s), preamp(s), envFollower(s)
    {
        setLookAndFeel(&lnf_);
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &drone1, &drone2, &drone4, &drone5, &drone3, &drone6,
                 &vcoA, &vcoB, &envA, &envB, &filter, &mixer, &effector,
                 &lfoA, &lfoB, &joystick, &seq, &preamp, &envFollower })
            addAndMakeVisible(c);
        setSize(kLogicalW, kLogicalH);
    }

    ~PanelView() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kCream);

        // Header print. Original branding replaces the wordmark before any
        // public build (trade-dress note in 08-implementation-plan.md).
        auto header = getLocalBounds().removeFromTop(layout::kHeader.h).reduced(60, 40);
        const juce::Font wordmark(juce::FontOptions(190.0f, juce::Font::bold));
        juce::AttributedString ws;
        ws.append("SOLAR 42", wordmark, kInk);
        ws.append(" N", wordmark, kAccentRed);
        ws.setJustification(juce::Justification::bottomLeft);
        ws.draw(g, header.toFloat());

        g.setColour(kInk);
        g.setFont(juce::FontOptions(96.0f, juce::Font::bold));
        g.drawFittedText("AMBIENT\nMACHINE", header, juce::Justification::centredRight, 2);
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
    }

private:
    SolarLookAndFeel lnf_;

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
};

} // namespace solar
