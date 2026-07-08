#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Telemetry.h"
#include "engine/keyboard/KbMenu.h"
#include "engine/keyboard/Quantiser.h"
#include "state/KeyboardState.h"
#include "ui/PanelSections.h"
#include "ui/SolarLookAndFeel.h"

namespace solar {

// The playable touch keyboard (M6): 12 capacitive plates, two transpose
// pushbuttons, the push-encoder + display (faithful firmware menu, KbMenu)
// and a shortcut into the modern settings drawer.
//
// Print layout (M9b P2, vs solar42n-panel-1.png): ridged plates clustered
// 6|6 around the centre control group — white icon-button row with the
// ▽/△ transpose buttons flanking the red-centre encoder, blue LCD below,
// sunburst logo print, △ outlines at the feet of each cluster's tall plate.
// Digital divergences kept on purpose: OCT readout (in the LCD status row)
// and the SETTINGS shortcut (top-right corner chip).
//
// Mouse model (plan §GUI):
//   - press a plate: touch; the Y position within the plate = pressure
//     (capacitive area, not force); dragging vertically works the pressure
//   - drag horizontally across plates = glissando
//   - Shift-click toggles a plate "sticky" (a spare finger — chords for the
//     arp, twin-side holds, legato)
//   - scroll while holding a plate = tune it (press-and-hold + rotate on the
//     hardware); semitone steps quantised, 0.0025 V steps microtonal
//   - encoder: drag vertically or scroll to rotate, click to select, hold
//     >=700 ms for the manual's 1 s long-press
class KeyboardSection : public juce::Component,
                        private juce::ChangeListener
{
public:
    KeyboardSection(KeyboardState& state, KbTouchState& touch)
        : state_(state), touch_(touch)
    {
        state_.addChangeListener(this);
    }

    ~KeyboardSection() override { state_.removeChangeListener(this); }

    std::function<void()> onOpenSettings;

    void setTelemetry(const s42::TelemetryData& t)
    {
        const bool platesChanged = t.kbHeld != tele_.kbHeld
                                   || std::abs(t.kbGateL - tele_.kbGateL) > 0.5f
                                   || std::abs(t.kbGateR - tele_.kbGateR) > 0.5f;
        const bool statusChanged = t.kbStep[0] != tele_.kbStep[0]
                                   || t.kbStep[1] != tele_.kbStep[1]
                                   || std::abs(t.kbExtClock - tele_.kbExtClock) > 0.5f
                                   || t.kbOctave != tele_.kbOctave
                                   || std::abs(t.kbOffset[0] - tele_.kbOffset[0]) > 0.5f
                                   || std::abs(t.kbOffset[1] - tele_.kbOffset[1]) > 0.5f;
        tele_ = t;
        if (platesChanged)
        {
            repaint(platesArea());
            repaint(buttonsArea()); // gate status rings live in the icon row
        }
        if (statusChanged)
        {
            repaint(displayRect());
            repaint(buttonsArea());
        }
    }

    // ------------------------------------------------------------- painting

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff17171a));
        g.fillRoundedRectangle(r, 24.0f);

        const auto cfg = state_.config();
        const bool splitPlates = cfg.behaviour != s42::KbBehaviour::Single;

        // Ridged plates (white outline + horizontal slats like the print).
        for (int p = 0; p < 12; ++p)
        {
            const auto pr = plateRect(p).toFloat();
            const bool held = (tele_.kbHeld >> p & 1) != 0 || p == activePlate_;
            if (held)
            {
                g.setColour(juce::Colour(0xfff6ecd2).withAlpha(0.30f));
                g.fillRoundedRectangle(pr, 10.0f);
            }
            if (sticky_[p])
            {
                g.setColour(kAccentRed.withAlpha(0.9f));
                g.drawRoundedRectangle(pr.expanded(7.0f), 12.0f, 4.0f);
            }
            g.setColour(juce::Colour(0xffe9e4d8).withAlpha(held ? 1.0f : 0.92f));
            g.drawRoundedRectangle(pr, 10.0f, 7.0f);
            const auto in = pr.reduced(15.0f, 18.0f);
            g.setColour(juce::Colour(0xffe9e4d8).withAlpha(held ? 0.95f : 0.72f));
            for (float y = in.getY(); y + 12.0f <= in.getBottom(); y += 27.0f)
                g.fillRect(in.getX(), y, in.getWidth(), 12.0f);
        }

        // △ outlines at the feet of each cluster's tall plate (print).
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.95f));
        for (int p : { 2, 9 })
        {
            const float cx = plateRect(p).toFloat().getCentreX();
            juce::Path t;
            t.addTriangle(cx, 596.0f, cx - 82.0f, 742.0f, cx + 82.0f, 742.0f);
            g.strokePath(t, juce::PathStrokeType(13.0f));
        }

        if (splitPlates) // twin/split: the centre group IS the 6|6 border
        {
            g.setColour(kAccentRed.withAlpha(0.85f));
            const float cx = kCentreX;
            const float dash[] = { 12.0f, 10.0f };
            g.drawDashedLine(juce::Line<float>(cx, 14.0f, cx, 98.0f), dash, 2, 3.5f);
            g.setFont(juce::FontOptions(36.0f, juce::Font::bold));
            g.drawText("L", juce::Rectangle<float>(cx - 68.0f, 26.0f, 44.0f, 44.0f),
                       juce::Justification::centred);
            g.drawText("R", juce::Rectangle<float>(cx + 24.0f, 26.0f, 44.0f, 44.0f),
                       juce::Justification::centred);
        }

        paintIconRow(g);
        paintTranspose(g);
        paintEncoder(g);
        paintDisplay(g, cfg);
        paintSunburst(g);

        // Settings shortcut (digital divergence: top-right corner chip).
        const auto s = setupRect().toFloat();
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.85f));
        g.drawRoundedRectangle(s, 12.0f, 3.0f);
        g.setFont(juce::FontOptions(36.0f, juce::Font::bold));
        g.drawText("SETTINGS", s, juce::Justification::centred);
    }

    // ---------------------------------------------------------------- mouse

    void mouseDown(const juce::MouseEvent& e) override
    {
        lastScreenPos_ = e.getScreenPosition().toFloat();
        dragMode_ = Drag::None;

        const int p = plateAt(e.getPosition());
        if (p >= 0)
        {
            if (e.mods.isShiftDown())
            {
                sticky_[p] = !sticky_[p];
                touch_.plate[p] = sticky_[p] ? kStickyPressure : 0.0f;
                repaint(platesArea());
                return;
            }
            dragMode_ = Drag::Plate;
            activePlate_ = p;
            touch_.plate[p] = pressureAt(p, e.getPosition());
            repaint(platesArea());
            return;
        }

        for (int b = 0; b < 2; ++b)
            if (transposeRect(b).contains(e.getPosition()))
            {
                dragMode_ = Drag::Button;
                heldButton_ = b;
                touch_.button[b] = true;
                repaint(buttonsArea());
                return;
            }

        if (encoderRect().contains(e.getPosition()))
        {
            dragMode_ = Drag::Encoder;
            encoderAccum_ = 0.0f;
            encoderMoved_ = false;
            pressMs_ = juce::Time::getMillisecondCounter();
            return;
        }

        if (setupRect().contains(e.getPosition()))
        {
            dragMode_ = Drag::Setup;
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        switch (dragMode_)
        {
            case Drag::Plate:
            {
                const int p = plateColumnAt(e.x);
                if (p >= 0 && p != activePlate_) // glissando across plates
                {
                    if (activePlate_ >= 0)
                        touch_.plate[activePlate_] = sticky_[activePlate_] ? kStickyPressure : 0.0f;
                    activePlate_ = p;
                }
                if (activePlate_ >= 0)
                    touch_.plate[activePlate_] = pressureAt(activePlate_, e.getPosition());
                repaint(platesArea());
                break;
            }
            case Drag::Encoder:
            {
                encoderAccum_ += (float) (lastScreenPos_.y - (float) e.getScreenY());
                while (std::abs(encoderAccum_) >= kDetentPx)
                {
                    const int step = encoderAccum_ > 0 ? 1 : -1;
                    encoderAccum_ -= (float) step * kDetentPx;
                    encoderMoved_ = true;
                    menuRotate(step);
                }
                break;
            }
            case Drag::None: // empty keyboard space pans, like the sections
                if (auto* s = findParentComponentOfClass<PanelSurface>())
                    s->panByScreenDelta(e.getScreenPosition().toFloat() - lastScreenPos_);
                break;
            default:
                break;
        }
        lastScreenPos_ = e.getScreenPosition().toFloat();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        switch (dragMode_)
        {
            case Drag::Plate:
                if (activePlate_ >= 0)
                    touch_.plate[activePlate_] = sticky_[activePlate_] ? kStickyPressure : 0.0f;
                activePlate_ = -1;
                repaint(platesArea());
                break;
            case Drag::Button:
                touch_.button[heldButton_] = false;
                repaint(buttonsArea());
                break;
            case Drag::Encoder:
                if (!encoderMoved_)
                {
                    const auto held = juce::Time::getMillisecondCounter() - pressMs_;
                    if (held >= 700)
                        menuLongPress();
                    else
                        menuClick();
                }
                break;
            case Drag::Setup:
                if (setupRect().contains(e.getPosition()) && onOpenSettings)
                    onOpenSettings();
                break;
            default:
                break;
        }
        dragMode_ = Drag::None;
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const int detents = wheel.deltaY > 0 ? 1 : -1;
        if (dragMode_ == Drag::Plate && activePlate_ >= 0)
        {
            // Press-and-hold a plate + rotate = tune it (manual p14).
            auto cfg = state_.config();
            s42::kbTunePlate(cfg, activePlate_, detents);
            state_.set(cfg);
            return;
        }
        if (encoderRect().contains(e.getPosition()))
        {
            menuRotate(detents);
            return;
        }
        Component::mouseWheelMove(e, wheel); // bubble to the editor (pan/zoom)
    }

private:
    enum class Drag { None, Plate, Button, Encoder, Setup };

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    static constexpr float kStickyPressure = 0.7f;
    static constexpr float kDetentPx = 26.0f;

    // ------------------------------------------------------------- geometry
    // Local space = 2235 x 800 logical panel units (layout::kKeyboard).
    // Plates sit 6|6 around the centre control column; tops stagger in the
    // print's wave (peak 3rd-from-outer edge on each side, inner plates low).

    static constexpr float kCentreX = 1117.5f;

    juce::Rectangle<int> plateRect(int p) const
    {
        static constexpr int top[12] = { 200, 136, 64, 136, 200, 272,
                                         272, 200, 136, 64, 136, 200 };
        const int x = p < 6 ? 40 + p * 142 : 1357 + (p - 6) * 142;
        return { x, top[p], 128, 480 };
    }

    juce::Rectangle<int> platesArea() const { return { 30, 50, 2180, 715 }; }
    juce::Rectangle<int> transposeRect(int b) const
    {
        return b == 0 ? juce::Rectangle<int>(830, 158, 104, 104)   // ▽ octave/offset down
                      : juce::Rectangle<int>(1300, 158, 104, 104); // △ up
    }
    juce::Rectangle<int> buttonsArea() const { return { 550, 10, 1200, 350 }; }
    juce::Rectangle<int> displayRect() const { return { 887, 372, 460, 196 }; }
    juce::Rectangle<int> encoderRect() const { return { 1022, 115, 190, 190 }; }
    juce::Rectangle<int> setupRect() const { return { 1955, 28, 240, 78 }; }

    // Status rings of the icon row: 3 left / 3 right of the transpose pair.
    juce::Rectangle<float> iconRingRect(int i) const
    {
        static constexpr float cx[6] = { 605.0f, 700.0f, 795.0f,
                                         1440.0f, 1535.0f, 1630.0f };
        return juce::Rectangle<float>(64.0f, 64.0f).withCentre({ cx[i], 95.0f });
    }

    int plateColumnAt(int x) const
    {
        for (int p = 0; p < 12; ++p)
        {
            const auto r = plateRect(p);
            if (x >= r.getX() - 8 && x <= r.getRight() + 8)
                return p;
        }
        return -1;
    }

    int plateAt(juce::Point<int> pos) const
    {
        for (int p = 0; p < 12; ++p)
            if (plateRect(p).expanded(6).contains(pos))
                return p;
        return -1;
    }

    float pressureAt(int p, juce::Point<int> pos) const
    {
        const auto r = plateRect(p);
        const float y01 = juce::jlimit(0.0f, 1.0f,
                                       ((float) pos.y - (float) r.getY()) / (float) r.getHeight());
        return 0.15f + 0.85f * y01; // top = light touch, bottom = full flesh
    }

    // ------------------------------------------------------ firmware bridge

    void menuRotate(int detents)
    {
        auto cfg = state_.config();
        auto pre = state_.presets();
        menu_.rotate(detents, cfg, pre);
        state_.setPresets(pre);
        state_.set(cfg);
        encoderAngle_ += 0.3f * (float) detents;
    }

    void menuClick()
    {
        auto cfg = state_.config();
        auto pre = state_.presets();
        menu_.click(cfg, pre);
        state_.setPresets(pre);
        state_.set(cfg);
    }

    void menuLongPress()
    {
        auto cfg = state_.config();
        auto pre = state_.presets();
        menu_.longPress(cfg, pre);
        state_.setPresets(pre);
        state_.set(cfg);
    }

    // ------------------------------------------------------------- painting

    // White status rings + printed glyphs (print's icon row; meanings bound
    // to available telemetry until the backlog item nails the manual's set):
    // left = gate L, gate R, ext clock; right = arp/seq step, offset L/R.
    void paintIconRow(juce::Graphics& g)
    {
        const bool lit[6] = { tele_.kbGateL > 0.5f, tele_.kbGateR > 0.5f,
                              tele_.kbExtClock > 0.5f, tele_.kbStep[0] >= 0,
                              tele_.kbOffset[0] > 0.5f, tele_.kbOffset[1] > 0.5f };
        for (int i = 0; i < 6; ++i)
        {
            const auto r = iconRingRect(i);
            if (lit[i])
            {
                g.setColour(juce::Colour(0xfff2df9a).withAlpha(0.55f));
                g.fillEllipse(r.expanded(9.0f));
            }
            g.setColour(juce::Colours::white.withAlpha(lit[i] ? 1.0f : 0.92f));
            g.fillEllipse(r);
            g.setColour(juce::Colour(0xffa89f8c));
            g.drawEllipse(r.reduced(1.0f), 2.5f);
            // Glyph prints down-right of its ring like the panel silkscreen.
            paintIconGlyph(g, i, { r.getRight() + 14.0f, r.getCentreY() + 28.0f });
        }
    }

    // Tiny silkscreen glyph beside a status ring, centred on `c`.
    void paintIconGlyph(juce::Graphics& g, int i, juce::Point<float> c)
    {
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.9f));
        juce::Path p;
        switch (i)
        {
            case 0: // clock face
                p.addEllipse(c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f);
                p.startNewSubPath(c.x, c.y);
                p.lineTo(c.x, c.y - 8.0f);
                p.startNewSubPath(c.x, c.y);
                p.lineTo(c.x + 6.0f, c.y + 3.0f);
                break;
            case 1: // lightning
                p.startNewSubPath(c.x + 6.0f, c.y - 14.0f);
                p.lineTo(c.x - 6.0f, c.y + 2.0f);
                p.lineTo(c.x + 2.0f, c.y + 2.0f);
                p.lineTo(c.x - 4.0f, c.y + 14.0f);
                break;
            case 2:
            case 3: // gate pulse
                p.startNewSubPath(c.x - 12.0f, c.y + 10.0f);
                p.lineTo(c.x - 4.0f, c.y + 10.0f);
                p.lineTo(c.x - 4.0f, c.y - 10.0f);
                p.lineTo(c.x + 6.0f, c.y - 10.0f);
                p.lineTo(c.x + 6.0f, c.y + 10.0f);
                p.lineTo(c.x + 12.0f, c.y + 10.0f);
                break;
            case 4: // ground arrow (offset down to the strip)
                p.startNewSubPath(c.x, c.y - 12.0f);
                p.lineTo(c.x, c.y + 8.0f);
                p.startNewSubPath(c.x - 7.0f, c.y + 1.0f);
                p.lineTo(c.x, c.y + 8.0f);
                p.lineTo(c.x + 7.0f, c.y + 1.0f);
                p.startNewSubPath(c.x - 9.0f, c.y + 13.0f);
                p.lineTo(c.x + 9.0f, c.y + 13.0f);
                break;
            case 5: // loop arrow
                p.addCentredArc(c.x, c.y, 10.0f, 10.0f, 0.0f, 0.6f, 5.4f, true);
                p.startNewSubPath(c.x + 2.0f, c.y - 15.0f);
                p.lineTo(c.x + 8.0f, c.y - 9.0f);
                p.lineTo(c.x + 1.0f, c.y - 4.0f);
                break;
        }
        g.strokePath(p, juce::PathStrokeType(3.0f));
    }

    void paintTranspose(juce::Graphics& g)
    {
        for (int b = 0; b < 2; ++b)
        {
            const auto r = transposeRect(b).toFloat();
            const bool pressed = dragMode_ == Drag::Button && heldButton_ == b;
            g.setColour(pressed ? juce::Colour(0xffcfcabd) : juce::Colours::white);
            g.fillEllipse(r);
            g.setColour(juce::Colour(0xff8f8574));
            g.drawEllipse(r.reduced(2.0f), 3.5f);
            // Offset-active LED (twin/split or microtonal single).
            const bool lit = tele_.kbOffset[b] > 0.5f;
            g.setColour(lit ? kAccentRed : kAccentRed.withAlpha(0.12f));
            g.fillEllipse(r.getCentreX() - 10.0f, r.getCentreY() - 10.0f, 20.0f, 20.0f);

            // ▽ / △ print above the button.
            juce::Path t;
            const float cx = r.getCentreX(), ty = 78.0f;
            if (b == 0)
                t.addTriangle(cx - 16.0f, ty - 14.0f, cx + 16.0f, ty - 14.0f, cx, ty + 14.0f);
            else
                t.addTriangle(cx, ty - 14.0f, cx - 16.0f, ty + 14.0f, cx + 16.0f, ty + 14.0f);
            g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.95f));
            g.strokePath(t, juce::PathStrokeType(4.0f));
        }
    }

    void paintDisplay(juce::Graphics& g, const s42::KbConfig& cfg)
    {
        const auto r = displayRect().toFloat();
        g.setColour(juce::Colour(0xff3b4bd8)); // hardware LCD is blue
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(juce::Colour(0xff141a4a));
        g.drawRoundedRectangle(r, 10.0f, 3.0f);

        const auto d = menu_.display(cfg);
        const juce::Colour lcd = juce::Colours::white;
        g.setColour(lcd);
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 46.0f,
                                    juce::Font::bold));
        g.drawText(d.line1, r.reduced(24.0f, 14.0f).removeFromTop(58.0f),
                   juce::Justification::centredLeft);
        g.setColour(d.editing ? lcd : lcd.withAlpha(0.8f));
        g.drawText(juce::String(d.editing ? "> " : "  ") + d.line2,
                   r.reduced(24.0f, 14.0f).withTrimmedTop(62.0f).removeFromTop(58.0f),
                   juce::Justification::centredLeft);

        // Status row: gates, clock source, octave transpose, arp/seq steps.
        auto status = r.reduced(24.0f, 10.0f).removeFromBottom(42.0f);
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.setColour(tele_.kbGateL > 0.5f ? lcd : lcd.withAlpha(0.3f));
        g.drawText("L", status.removeFromLeft(40.0f), juce::Justification::centredLeft);
        g.setColour(tele_.kbGateR > 0.5f ? lcd : lcd.withAlpha(0.3f));
        g.drawText("R", status.removeFromLeft(40.0f), juce::Justification::centredLeft);
        g.setColour(tele_.kbExtClock > 0.5f ? lcd : lcd.withAlpha(0.3f));
        g.drawText("EXT", status.removeFromLeft(80.0f), juce::Justification::centredLeft);
        if (cfg.behaviour == s42::KbBehaviour::Single && cfg.scaleMask != 0)
        {
            g.setColour(lcd.withAlpha(0.85f));
            g.drawText(juce::String::formatted("OCT %+d", (int) tele_.kbOctave),
                       status.removeFromLeft(120.0f), juce::Justification::centredLeft);
        }
        if (tele_.kbStep[0] >= 0)
        {
            g.setColour(lcd.withAlpha(0.85f));
            g.drawText("STEP " + juce::String(tele_.kbStep[0] + 1), status,
                       juce::Justification::centredRight);
        }
    }

    void paintEncoder(juce::Graphics& g)
    {
        const auto r = encoderRect().toFloat();
        const auto c = r.getCentre();
        const float rad = r.getWidth() * 0.5f;

        // Grey bezel, dark knurled body, red ring + red centre (print).
        g.setColour(juce::Colour(0xffb9b5ab));
        g.fillEllipse(r);
        g.setColour(juce::Colour(0xff8f8574));
        g.drawEllipse(r.reduced(1.5f), 3.0f);
        g.setColour(juce::Colour(0xff232327));
        g.fillEllipse(c.x - rad * 0.84f, c.y - rad * 0.84f, rad * 1.68f, rad * 1.68f);
        g.setColour(juce::Colour(0xff4a4a50));
        for (int k = 0; k < 16; ++k) // knurling, rotates with the detents
        {
            const float a = encoderAngle_ + juce::MathConstants<float>::twoPi * (float) k / 16.0f;
            const auto p1 = c.getPointOnCircumference(rad * 0.80f, a);
            const auto p2 = c.getPointOnCircumference(rad * 0.62f, a);
            g.drawLine({ p1, p2 }, 4.0f);
        }
        g.setColour(kAccentRed);
        g.fillEllipse(c.x - rad * 0.55f, c.y - rad * 0.55f, rad * 1.1f, rad * 1.1f);
        g.setColour(kAccentRed.darker(0.4f));
        g.drawEllipse(c.x - rad * 0.55f, c.y - rad * 0.55f, rad * 1.1f, rad * 1.1f, 6.0f);
        const auto dot = c.getPointOnCircumference(rad * 0.36f, encoderAngle_);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillEllipse(dot.x - 8.0f, dot.y - 8.0f, 16.0f, 16.0f);

        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.5f));
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        g.drawText("push", juce::Rectangle<float>(c.x - 80.0f, r.getBottom() + 8.0f, 160.0f, 30.0f),
                   juce::Justification::centred);
    }

    // Elta sunburst logo print between the LCD and the block's bottom edge.
    void paintSunburst(juce::Graphics& g)
    {
        const juce::Point<float> c(kCentreX, 680.0f);
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.95f));
        g.fillEllipse(c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
        for (float rr : { 22.0f, 40.0f, 58.0f })
            g.drawEllipse(c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 8.0f);

        juce::Path flames; // irregular spikes, deterministic (no RNG in paint)
        constexpr int n = 14;
        constexpr float step = juce::MathConstants<float>::twoPi / (float) n;
        for (int k = 0; k < n; ++k)
        {
            const float a = (float) k * step;
            const float rOut = 92.0f + 20.0f * (0.5f + 0.5f * std::sin((float) k * 2.1f));
            flames.addTriangle(c.getPointOnCircumference(72.0f, a),
                               c.getPointOnCircumference(rOut, a + step * 0.35f),
                               c.getPointOnCircumference(72.0f, a + step));
        }
        g.fillPath(flames);
        g.drawEllipse(c.x - 70.0f, c.y - 70.0f, 140.0f, 140.0f, 9.0f);
    }

    KeyboardState& state_;
    KbTouchState& touch_;
    s42::KbMenu menu_;
    s42::TelemetryData tele_ {};

    Drag dragMode_ = Drag::None;
    int activePlate_ = -1;
    bool sticky_[12] = {};
    int heldButton_ = 0;
    float encoderAccum_ = 0.0f, encoderAngle_ = 0.0f;
    bool encoderMoved_ = false;
    uint32_t pressMs_ = 0;
    juce::Point<float> lastScreenPos_;
};

} // namespace solar
