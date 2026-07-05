#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "state/PatchBay.h"
#include "ui/PanelLayout.h"
#include "ui/SolarLookAndFeel.h"
#include "ui/Tooltips.h"

namespace solar {

// ---------------------------------------------------------------- jack print
// Static layer: a hex-nut jack + label for every entry in the layout census.
// Hardware label code: red = outputs, ink = inputs. Sits above the sections
// (jacks live inside section rectangles) and never takes the mouse — the
// CableLayer on top owns all jack interaction.
class JackLayer : public juce::Component
{
public:
    JackLayer() { setInterceptsMouseClicks(false, false); }

    void paint(juce::Graphics& g) override
    {
        for (const auto& j : layout::kJacks)
        {
            const auto c = juce::Point<float>((float) j.x, (float) j.y);
            const float r = (float) layout::kJackR;

            juce::Path nut;
            nut.addPolygon(c, 6, r * 1.55f, juce::MathConstants<float>::pi / 6.0f);
            g.setColour(juce::Colour(0xffcfccc2));
            g.fillPath(nut);
            g.setColour(kInk.withAlpha(0.65f));
            g.strokePath(nut, juce::PathStrokeType(3.0f));

            g.setColour(juce::Colour(0xff3a3a40));
            g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            g.setColour(juce::Colours::black);
            const float hole = r * 0.62f;
            g.fillEllipse(c.x - hole, c.y - hole, hole * 2.0f, hole * 2.0f);

            g.setColour(j.isInlet ? kInk : kAccentRed);
            g.setFont(juce::FontOptions(34.0f, juce::Font::bold));
            g.drawText(j.label,
                       juce::Rectangle<float>(260.0f, 40.0f)
                           .withCentre({ c.x, c.y + r * 1.55f + 26.0f }),
                       juce::Justification::centred);
        }
    }
};

// ------------------------------------------------------------------- cables
// Full virtual patching (plan §GUI): press a jack to pull a ghost cable,
// compatible jacks light up, release to connect. Grab a plug to re-route
// (dropping it in space unplugs, like hardware). Alt-click unplugs. One cable
// per inlet, any number per outlet. Unpatched inlets show their hardware
// normal as a dashed trace on hover.
class CableLayer : public juce::Component,
                   public juce::TooltipClient,
                   private juce::ChangeListener
{
public:
    explicit CableLayer(PatchBay& bay) : bay_(bay)
    {
        setOpaque(false);
        bay_.addChangeListener(this);
    }

    ~CableLayer() override { bay_.removeChangeListener(this); }

    // The hovered jack explains itself from the registry (M7): direction,
    // voltage range, and which normal a cable would break.
    juce::String getTooltip() override
    {
        if (dragging_ || hover_ < 0)
            return {};
        const auto& j = layout::kJacks[hover_];
        return tips::jackTooltip(j.isInlet, j.index);
    }

    bool hitTest(int x, int y) override
    {
        return dragging_ || jackAt({ (float) x, (float) y }) >= 0;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int j = jackAt(e.position);
        if (j < 0)
            return;
        const auto& jack = layout::kJacks[j];

        if (jack.isInlet)
        {
            const auto in = (s42::Inlet) jack.index;
            const int src = bay_.sourceOf(in);
            if (e.mods.isAltDown())
            {
                if (src >= 0)
                    bay_.disconnect(in);
                return;
            }
            if (src >= 0)
            {
                // Grab the plug: the cable comes loose and dangles from its
                // outlet until dropped on another inlet (or into space).
                bay_.disconnect(in);
                startDrag(jackIndexOf(false, src), e.position);
                return;
            }
        }
        startDrag(j, e.position);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging_)
            return;
        dragPos_ = e.position;
        const int t = jackAt(e.position);
        target_ = (t >= 0 && compatible(dragFrom_, t)) ? t : -1;
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging_ && target_ >= 0)
        {
            const auto& a = layout::kJacks[dragFrom_];
            const auto& b = layout::kJacks[target_];
            const auto& in = a.isInlet ? a : b;
            const auto& out = a.isInlet ? b : a;
            bay_.connect((s42::Inlet) in.index, (s42::Outlet) out.index);
        }
        dragging_ = false;
        target_ = -1;
        repaint();
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int j = jackAt(e.position);
        if (j != hover_)
        {
            hover_ = j;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hover_ >= 0)
        {
            hover_ = -1;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        // Hover: ring the jack; unpatched inlets also reveal their normal.
        if (hover_ >= 0 && !dragging_)
        {
            ringJack(g, hover_, kAccentRed.withAlpha(0.75f));
            drawNormalTrace(g, hover_);
        }

        // Ghost + candidate highlights while dragging.
        if (dragging_)
        {
            for (int j = 0; j < layout::kNumJacks; ++j)
                if (compatible(dragFrom_, j))
                    ringJack(g, j, kAccentRed.withAlpha(j == target_ ? 0.9f : 0.3f));

            const auto from = posOf(dragFrom_);
            const auto to = target_ >= 0 ? posOf(target_) : dragPos_;
            drawCable(g, from, to, cableColour(dragFrom_).withAlpha(0.55f));
        }

        // The patch itself.
        for (int i = 0; i < s42::kInletCount; ++i)
        {
            const int src = bay_.sourceOf((s42::Inlet) i);
            if (src < 0)
                continue;
            const int inJack = jackIndexOf(true, i);
            const int outJack = jackIndexOf(false, src);
            if (inJack >= 0 && outJack >= 0)
                drawCable(g, posOf(outJack), posOf(inJack),
                          cableColour(inJack).withAlpha(0.85f));
        }
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    void startDrag(int fromJack, juce::Point<float> pos)
    {
        if (fromJack < 0)
            return;
        dragging_ = true;
        dragFrom_ = fromJack;
        dragPos_ = pos;
        target_ = -1;
        repaint();
    }

    static juce::Point<float> posOf(int j)
    {
        return { (float) layout::kJacks[j].x, (float) layout::kJacks[j].y };
    }

    static int jackIndexOf(bool isInlet, int index)
    {
        for (int j = 0; j < layout::kNumJacks; ++j)
            if (layout::kJacks[j].isInlet == isInlet && layout::kJacks[j].index == index)
                return j;
        return -1;
    }

    static int jackAt(juce::Point<float> p)
    {
        int best = -1;
        float bestD = (float) layout::kJackHitR;
        for (int j = 0; j < layout::kNumJacks; ++j)
        {
            const float d = posOf(j).getDistanceFrom(p);
            if (d < bestD)
            {
                bestD = d;
                best = j;
            }
        }
        return best;
    }

    static bool compatible(int a, int b)
    {
        return a >= 0 && b >= 0 && layout::kJacks[a].isInlet != layout::kJacks[b].isInlet;
    }

    static juce::Colour cableColour(int jack)
    {
        static const juce::Colour palette[] = {
            juce::Colour(0xffc8271e), juce::Colour(0xff17171a), juce::Colour(0xff2e7d9c),
            juce::Colour(0xff2e7d4f), juce::Colour(0xffe08a2e), juce::Colour(0xff6a4fa3),
        };
        return palette[(size_t) jack % std::size(palette)];
    }

    void ringJack(juce::Graphics& g, int j, juce::Colour c)
    {
        const auto p = posOf(j);
        const float r = (float) layout::kJackR * 1.9f;
        g.setColour(c);
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, 9.0f);
    }

    void drawNormalTrace(juce::Graphics& g, int j)
    {
        const auto& jack = layout::kJacks[j];
        if (!jack.isInlet || bay_.sourceOf((s42::Inlet) jack.index) >= 0)
            return;
        const auto& info = s42::kInletInfo[jack.index];
        if (info.kind != s42::NormalKind::FromOutlet)
            return;
        const int outJack = jackIndexOf(false, (int) info.normalOutlet);
        if (outJack < 0)
            return;

        juce::Path p = cablePath(posOf(outJack), posOf(j));
        juce::Path dashed;
        const float dashes[] = { 22.0f, 26.0f };
        juce::PathStrokeType(6.0f).createDashedStroke(dashed, p, dashes, 2);
        g.setColour(kInk.withAlpha(0.35f));
        g.fillPath(dashed);
        ringJack(g, outJack, kInk.withAlpha(0.35f));
    }

    static juce::Path cablePath(juce::Point<float> a, juce::Point<float> b)
    {
        // Gravity sag grows with span, capped so short patches stay tidy.
        const float sag = juce::jmin(320.0f, a.getDistanceFrom(b) * 0.28f) + 30.0f;
        juce::Path p;
        p.startNewSubPath(a);
        p.cubicTo({ a.x + (b.x - a.x) * 0.25f, a.y + (b.y - a.y) * 0.25f + sag },
                  { a.x + (b.x - a.x) * 0.75f, a.y + (b.y - a.y) * 0.75f + sag }, b);
        return p;
    }

    static void drawCable(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                          juce::Colour colour)
    {
        const auto path = cablePath(a, b);
        g.setColour(juce::Colours::black.withAlpha(0.25f * colour.getFloatAlpha()));
        g.strokePath(path, juce::PathStrokeType(26.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(18.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Plug bodies.
        for (const auto& end : { a, b })
        {
            g.setColour(juce::Colour(0xff222226).withAlpha(colour.getFloatAlpha()));
            g.fillEllipse(end.x - 30.0f, end.y - 30.0f, 60.0f, 60.0f);
            g.setColour(juce::Colour(0xff8f8c84).withAlpha(colour.getFloatAlpha()));
            g.fillEllipse(end.x - 14.0f, end.y - 14.0f, 28.0f, 28.0f);
        }
    }

    PatchBay& bay_;
    bool dragging_ = false;
    int dragFrom_ = -1, target_ = -1, hover_ = -1;
    juce::Point<float> dragPos_;
};

} // namespace solar
