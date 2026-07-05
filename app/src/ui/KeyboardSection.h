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
            repaint(platesArea());
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

        // Plates.
        for (int p = 0; p < 12; ++p)
        {
            const auto pr = plateRect(p).toFloat();
            const bool held = (tele_.kbHeld >> p & 1) != 0 || p == activePlate_;
            if (held)
            {
                g.setColour(juce::Colour(0xfff6ecd2).withAlpha(0.28f));
                g.fillRoundedRectangle(pr, 14.0f);
            }
            if (sticky_[p])
            {
                g.setColour(kAccentRed.withAlpha(0.9f));
                g.drawRoundedRectangle(pr.expanded(7.0f), 16.0f, 4.0f);
            }
            g.setColour(juce::Colour(0xffe9e4d8).withAlpha(held ? 1.0f : 0.9f));
            g.drawRoundedRectangle(pr, 14.0f, 5.0f);
        }
        if (splitPlates) // twin/split: mark the 6|6 border
        {
            g.setColour(kAccentRed.withAlpha(0.8f));
            const float x = (plateRect(5).getRight() + plateRect(6).getX()) * 0.5f;
            const float dash[] = { 14.0f, 14.0f };
            g.drawDashedLine(juce::Line<float>(x, 60.0f, x, (float) getHeight() - 60.0f),
                             dash, 2, 3.0f);
        }

        paintTranspose(g, cfg);
        paintDisplay(g, cfg);
        paintEncoder(g);

        // Settings shortcut.
        const auto s = setupRect().toFloat();
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.85f));
        g.drawRoundedRectangle(s, 12.0f, 3.0f);
        g.setFont(juce::FontOptions(40.0f, juce::Font::bold));
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

    juce::Rectangle<int> plateRect(int p) const
    {
        const float x0 = 190.0f, x1 = 1740.0f;
        const float slot = (x1 - x0) / 12.0f;
        const float w = slot * 0.74f;
        const float tall = (p % 2 == 0) ? 0.62f : 0.78f;
        const float h = 720.0f * tall;
        return juce::Rectangle<float>(x0 + slot * (float) p + (slot - w) * 0.5f,
                                      40.0f + (720.0f - h) * 0.5f, w, h)
            .toNearestInt();
    }

    juce::Rectangle<int> platesArea() const { return { 160, 20, 1620, 770 }; }
    juce::Rectangle<int> transposeRect(int b) const
    {
        return b == 0 ? juce::Rectangle<int>(30, 230, 120, 120)
                      : juce::Rectangle<int>(30, 450, 120, 120);
    }
    juce::Rectangle<int> buttonsArea() const { return { 20, 200, 150, 430 }; }
    juce::Rectangle<int> displayRect() const { return { 1790, 90, 405, 230 }; }
    juce::Rectangle<int> encoderRect() const { return { 1875, 360, 240, 240 }; }
    juce::Rectangle<int> setupRect() const { return { 1790, 680, 405, 90 }; }

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

    void paintTranspose(juce::Graphics& g, const s42::KbConfig& cfg)
    {
        for (int b = 0; b < 2; ++b)
        {
            const auto r = transposeRect(b).toFloat();
            g.setColour(juce::Colour(0xff2b2b30));
            g.fillEllipse(r);
            g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.9f));
            g.drawEllipse(r.reduced(3.0f), 4.0f);
            // Offset-active LED (twin/split or microtonal single).
            const bool lit = tele_.kbOffset[b] > 0.5f;
            g.setColour(lit ? kAccentRed : kAccentRed.withAlpha(0.15f));
            g.fillEllipse(r.getCentreX() - 12.0f, r.getCentreY() - 12.0f, 24.0f, 24.0f);
        }
        // Octave readout between the buttons (single + quantiser).
        if (cfg.behaviour == s42::KbBehaviour::Single && cfg.scaleMask != 0)
        {
            g.setColour(juce::Colour(0xffe9e4d8));
            g.setFont(juce::FontOptions(38.0f, juce::Font::bold));
            g.drawText(juce::String::formatted("OCT %+d", (int) tele_.kbOctave),
                       juce::Rectangle<int>(10, 370, 160, 60), juce::Justification::centred);
        }
    }

    void paintDisplay(juce::Graphics& g, const s42::KbConfig& cfg)
    {
        const auto r = displayRect().toFloat();
        g.setColour(juce::Colour(0xff0a0c0a));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(juce::Colour(0xff3a3f38));
        g.drawRoundedRectangle(r, 10.0f, 3.0f);

        const auto d = menu_.display(cfg);
        const juce::Colour lcd(0xff9fe870);
        g.setColour(lcd);
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 46.0f,
                                    juce::Font::bold));
        g.drawText(d.line1, r.reduced(24.0f, 16.0f).removeFromTop(60.0f),
                   juce::Justification::centredLeft);
        g.setColour(d.editing ? juce::Colour(0xffd7ff9e) : lcd.withAlpha(0.85f));
        g.drawText(juce::String(d.editing ? "> " : "  ") + d.line2,
                   r.reduced(24.0f, 16.0f).withTrimmedTop(64.0f).removeFromTop(60.0f),
                   juce::Justification::centredLeft);

        // Status row: gates, clock source, arp/seq steps.
        auto status = r.reduced(24.0f, 12.0f).removeFromBottom(44.0f);
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.setColour(tele_.kbGateL > 0.5f ? lcd : lcd.withAlpha(0.25f));
        g.drawText("L", status.removeFromLeft(40.0f), juce::Justification::centredLeft);
        g.setColour(tele_.kbGateR > 0.5f ? lcd : lcd.withAlpha(0.25f));
        g.drawText("R", status.removeFromLeft(40.0f), juce::Justification::centredLeft);
        g.setColour(tele_.kbExtClock > 0.5f ? lcd : lcd.withAlpha(0.25f));
        g.drawText("EXT", status.removeFromLeft(80.0f), juce::Justification::centredLeft);
        if (tele_.kbStep[0] >= 0)
        {
            g.setColour(lcd.withAlpha(0.85f));
            g.drawText("STEP " + juce::String(tele_.kbStep[0] + 1), status,
                       juce::Justification::centredRight);
        }
    }

    void paintEncoder(juce::Graphics& g)
    {
        const auto r = encoderRect().toFloat().reduced(10.0f);
        const auto c = r.getCentre();
        const float rad = r.getWidth() * 0.5f;
        g.setColour(juce::Colour(0xff2b2b30));
        g.fillEllipse(r);
        g.setColour(juce::Colour(0xff47474d));
        g.fillEllipse(c.x - rad * 0.62f, c.y - rad * 0.62f, rad * 1.24f, rad * 1.24f);
        g.setColour(juce::Colour(0xffe9e4d8).withAlpha(0.9f));
        g.drawEllipse(r, 4.0f);
        for (int k = 0; k < 12; ++k) // knurling
        {
            const float a = encoderAngle_ + juce::MathConstants<float>::twoPi * (float) k / 12.0f;
            const auto p1 = c.getPointOnCircumference(rad * 0.92f, a);
            const auto p2 = c.getPointOnCircumference(rad * 0.72f, a);
            g.drawLine({ p1, p2 }, 3.0f);
        }
        const auto dot = c.getPointOnCircumference(rad * 0.45f, encoderAngle_);
        g.setColour(juce::Colour(0xffe9e4d8));
        g.fillEllipse(dot.x - 10.0f, dot.y - 10.0f, 20.0f, 20.0f);
        g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
        g.drawText("PUSH", juce::Rectangle<float>(c.x - 80.0f, r.getBottom() + 2.0f, 160.0f, 36.0f),
                   juce::Justification::centred);
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
