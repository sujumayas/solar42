#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/keyboard/KbConfig.h"
#include "engine/keyboard/Quantiser.h"
#include "state/KeyboardState.h"

namespace solar {

// The modern, non-skeuomorphic path to the keyboard firmware (plan §GUI: a
// parallel SettingsDrawer exposing the same state as the encoder menu).
// Desktop-space overlay on the editor's right edge; every control writes the
// KEYBOARD state tree through KeyboardState, so the encoder display, the
// drawer and the engine always agree.
class SettingsDrawer : public juce::Component,
                       private juce::ChangeListener
{
public:
    SettingsDrawer(KeyboardState& state, juce::AudioProcessorValueTreeState& apvts)
        : state_(state), apvts_(apvts)
    {
        title_.setText("TOUCH KEYBOARD", juce::dontSendNotification);
        title_.setFont(juce::FontOptions(17.0f, juce::Font::bold));
        addAndMakeVisible(title_);
        close_.setButtonText("x");
        close_.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible(close_);

        viewport_.setViewedComponent(&content_, false);
        viewport_.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport_);

        buildContent();
        state_.addChangeListener(this);
        refresh();
    }

    ~SettingsDrawer() override { state_.removeChangeListener(this); }

    std::function<void()> onClose;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff232327));
        g.setColour(juce::Colour(0xff17171a));
        g.fillRect(getLocalBounds().removeFromTop(34));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop(34);
        close_.setBounds(head.removeFromRight(34).reduced(5));
        title_.setBounds(head.reduced(10, 0));
        viewport_.setBounds(r);
        content_.setSize(r.getWidth() - viewport_.getScrollBarThickness(), contentHeight_);
        layoutContent();
    }

private:
    // ------------------------------------------------------- custom widgets

    // Clickable bit-mask cells (scale editor, rhythm masks).
    struct MaskEditor : juce::Component
    {
        uint16_t mask = 0;
        int cells = 8;
        std::function<juce::String(int)> cellLabel;
        std::function<void(uint16_t)> onChange;

        void set(uint16_t m, int n)
        {
            mask = m;
            cells = n;
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            const float w = (float) getWidth() / (float) cells;
            for (int i = 0; i < cells; ++i)
            {
                auto c = juce::Rectangle<float>(w * (float) i, 0.0f, w, (float) getHeight())
                             .reduced(1.5f);
                const bool on = (mask >> i & 1) != 0;
                g.setColour(on ? juce::Colour(0xffe08a2e) : juce::Colour(0xff2f2f34));
                g.fillRoundedRectangle(c, 3.0f);
                g.setColour(on ? juce::Colours::black : juce::Colours::white.withAlpha(0.5f));
                g.setFont(juce::FontOptions(10.0f));
                if (cellLabel)
                    g.drawText(cellLabel(i), c, juce::Justification::centred);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            const int i = juce::jlimit(0, cells - 1, e.x * cells / juce::jmax(1, getWidth()));
            mask = (uint16_t) (mask ^ (1u << i));
            repaint();
            if (onChange)
                onChange(mask);
        }
    };

    // 16-step value bars + gate cells for the keyboard sequencer.
    struct StepEditor : juce::Component
    {
        s42::SeqConfig seq {};
        std::function<void(int, float)> onValue;
        std::function<void(int, bool)> onGate;

        void set(const s42::SeqConfig& s)
        {
            seq = s;
            repaint();
        }

        static constexpr int kGateRow = 18;

        void paint(juce::Graphics& g) override
        {
            const float w = (float) getWidth() / 16.0f;
            const float barsH = (float) (getHeight() - kGateRow);
            for (int i = 0; i < 16; ++i)
            {
                const bool inLen = i < seq.length;
                auto col = juce::Rectangle<float>(w * (float) i, 0.0f, w, barsH).reduced(1.5f, 0.0f);
                g.setColour(juce::Colour(inLen ? 0xff2f2f34 : 0xff26262a));
                g.fillRect(col);
                const float v01 = juce::jlimit(0.0f, 1.0f, seq.value[i] * 0.5f);
                auto bar = col.removeFromBottom(juce::jmax(2.0f, v01 * barsH));
                g.setColour(inLen ? juce::Colour(0xff4fa3e0) : juce::Colour(0xff3a4a58));
                g.fillRect(bar);

                auto gate = juce::Rectangle<float>(w * (float) i, barsH + 2.0f, w,
                                                   (float) kGateRow - 4.0f)
                                .reduced(1.5f, 0.0f);
                g.setColour(seq.gate[i] ? juce::Colour(0xff3ac86a) : juce::Colour(0xff2f2f34));
                g.fillRoundedRectangle(gate, 2.0f);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            gateDrag_ = e.y > getHeight() - kGateRow;
            if (gateDrag_)
            {
                const int i = stepAt(e.x);
                seq.gate[i] = !seq.gate[i];
                repaint();
                if (onGate)
                    onGate(i, seq.gate[i]);
            }
            else
                mouseDrag(e);
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (gateDrag_)
                return;
            const int i = stepAt(e.x);
            const float barsH = (float) (getHeight() - kGateRow);
            const float v = juce::jlimit(0.0f, 2.0f,
                                         (1.0f - (float) e.y / barsH) * 2.0f);
            seq.value[i] = v;
            repaint();
            if (onValue)
                onValue(i, v);
        }

        int stepAt(int x) const
        {
            return juce::jlimit(0, 15, x * 16 / juce::jmax(1, getWidth()));
        }

        bool gateDrag_ = false;
    };

    // ------------------------------------------------------------ row plumbing

    enum class RowKind { Header, Widget, Tall };

    struct Row
    {
        RowKind kind;
        std::unique_ptr<juce::Label> label;
        juce::Component* widget = nullptr; // owned by the specific vectors below
        int height = 26;
        bool sideScoped = false; // dim when the R tab exists but split is off
    };

    juce::Label* makeLabel(const juce::String& text, float size, bool bold)
    {
        auto* l = new juce::Label({}, text);
        l->setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
        l->setColour(juce::Label::textColourId,
                     bold ? juce::Colour(0xffe08a2e) : juce::Colours::white.withAlpha(0.85f));
        content_.addAndMakeVisible(l);
        return l;
    }

    void addHeader(const juce::String& text)
    {
        Row r;
        r.kind = RowKind::Header;
        r.label.reset(makeLabel(text, 13.0f, true));
        r.height = 30;
        rows_.push_back(std::move(r));
    }

    void addRow(const juce::String& label, juce::Component* widget, int height = 26)
    {
        Row r;
        r.kind = height > 40 ? RowKind::Tall : RowKind::Widget;
        if (label.isNotEmpty())
            r.label.reset(makeLabel(label, 12.5f, false));
        r.widget = widget;
        r.height = height;
        rows_.push_back(std::move(r));
    }

    using Cfg = s42::KbConfig;

    juce::ComboBox* addCombo(const juce::String& label, const juce::StringArray& items,
                             std::function<void(Cfg&, int)> apply,
                             std::function<int(const Cfg&)> read)
    {
        auto* box = new juce::ComboBox();
        box->addItemList(items, 1);
        box->onChange = [this, box, apply]
        {
            if (loading_)
                return;
            auto c = state_.config();
            apply(c, box->getSelectedItemIndex());
            state_.set(c);
        };
        combos_.add(box);
        content_.addAndMakeVisible(box);
        refreshers_.push_back([this, box, read]
                              { box->setSelectedItemIndex(read(state_.config()),
                                                          juce::dontSendNotification); });
        addRow(label, box);
        return box;
    }

    juce::Slider* addSlider(const juce::String& label, double lo, double hi, double step,
                            std::function<void(Cfg&, double)> apply,
                            std::function<double(const Cfg&)> read)
    {
        auto* s = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        s->setRange(lo, hi, step);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
        s->onValueChange = [this, s, apply]
        {
            if (loading_)
                return;
            auto c = state_.config();
            apply(c, s->getValue());
            state_.set(c);
        };
        sliders_.add(s);
        content_.addAndMakeVisible(s);
        refreshers_.push_back([this, s, read]
                              { s->setValue(read(state_.config()), juce::dontSendNotification); });
        addRow(label, s);
        return s;
    }

    juce::ToggleButton* addToggle(const juce::String& label,
                                  std::function<void(Cfg&, bool)> apply,
                                  std::function<bool(const Cfg&)> read)
    {
        auto* t = new juce::ToggleButton(label);
        t->onClick = [this, t, apply]
        {
            if (loading_)
                return;
            auto c = state_.config();
            apply(c, t->getToggleState());
            state_.set(c);
        };
        toggles_.add(t);
        content_.addAndMakeVisible(t);
        refreshers_.push_back([this, t, read]
                              { t->setToggleState(read(state_.config()),
                                                  juce::dontSendNotification); });
        addRow({}, t);
        return t;
    }

    // The side (L/R) the side-scoped controls edit. Twin shares side 0.
    int sideIx() const
    {
        return state_.config().behaviour == s42::KbBehaviour::Split ? sideTab_ : 0;
    }

    s42::KbSideConfig& scopedSide(Cfg& c) const { return c.side[sideIx()]; }
    const s42::KbSideConfig& scopedSide(const Cfg& c) const { return c.side[sideIx()]; }

    // --------------------------------------------------------------- content

    void buildContent()
    {
        const juce::StringArray dirs { "Forward", "Backward", "Ping-pong", "Random" };
        const juce::StringArray ratios { "/8", "/4", "/2", "x1", "x2", "x4", "x8" };

        addHeader("BEHAVIOUR / CLOCK");
        addCombo("Behaviour", { "Single", "Twin", "Split" },
                 [](Cfg& c, int v) { c.behaviour = (s42::KbBehaviour) v; },
                 [](const Cfg& c) { return (int) c.behaviour; });
        addSlider("Clock BPM", 10, 300, 1,
                  [](Cfg& c, double v)
                  {
                      c.bpm = (float) v;
                      ++c.bpmEdit; // manual: editing the tempo reverts to internal clock
                  },
                  [](const Cfg& c) { return (double) c.bpm; });

        addHeader("QUANTISER");
        scaleBox_ = new juce::ComboBox();
        scaleBox_->addItemList(scaleNames(), 1);
        scaleBox_->setSelectedItemIndex(0, juce::dontSendNotification);
        combos_.add(scaleBox_);
        content_.addAndMakeVisible(scaleBox_);
        addRow("Scale", scaleBox_);
        auto* load = new juce::TextButton("Load scale (retunes plates)");
        load->onClick = [this]
        {
            auto c = state_.config();
            s42::kbLoadScale(c, scaleBox_->getSelectedItemIndex());
            state_.set(c);
        };
        buttons_.add(load);
        content_.addAndMakeVisible(load);
        addRow({}, load);
        addCombo("Root", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "B", "H" },
                 [](Cfg& c, int v) { c.rootNote = v; },
                 [](const Cfg& c) { return c.rootNote; });
        scaleMask_.cells = 12;
        scaleMask_.cellLabel = [this](int i)
        { return juce::String(s42::kKbRootNames[(state_.config().rootNote + i) % 12]); };
        scaleMask_.onChange = [this](uint16_t m)
        {
            auto c = state_.config();
            c.scaleMask = m & 0x0FFF;
            state_.set(c);
        };
        content_.addAndMakeVisible(scaleMask_);
        addRow("Notes (none = microtonal)", &scaleMask_, 30);

        addHeader("PLAY");
        addSlider("Portamento", 0, 255, 1,
                  [](Cfg& c, double v) { c.portamento = (int) v; },
                  [](const Cfg& c) { return (double) c.portamento; });
        addToggle("Legato glide (2+ plates)",
                  [](Cfg& c, bool v) { c.legato = v; },
                  [](const Cfg& c) { return c.legato; });
        addSlider("Vibrato speed", 0, 127, 1,
                  [](Cfg& c, double v) { c.vibSpeed = (int) v; },
                  [](const Cfg& c) { return (double) c.vibSpeed; });
        addSlider("Vibrato depth", 0, 127, 1,
                  [](Cfg& c, double v) { c.vibDepth = (int) v; },
                  [](const Cfg& c) { return (double) c.vibDepth; });
        addSlider("Vibrato delay", 0, 127, 1,
                  [](Cfg& c, double v) { c.vibDelay = (int) v; },
                  [](const Cfg& c) { return (double) c.vibDelay; });
        addToggle("Pressure controls vibrato",
                  [](Cfg& c, bool v) { c.vibPressure = v; },
                  [](const Cfg& c) { return c.vibPressure; });
        addCombo("Pressure output", { "Pressure", "ASR", "AD", "Loop AD", "Random" },
                 [](Cfg& c, int v) { c.pressureMode = (s42::KbPressureMode) v; },
                 [](const Cfg& c) { return (int) c.pressureMode; });
        addSlider("Rise", 0, 255, 1,
                  [](Cfg& c, double v) { c.rise = (int) v; },
                  [](const Cfg& c) { return (double) c.rise; });
        addSlider("Fall", 0, 255, 1,
                  [](Cfg& c, double v) { c.fall = (int) v; },
                  [](const Cfg& c) { return (double) c.fall; });

        addHeader("SIDE (SPLIT: L / R)");
        for (int t = 0; t < 2; ++t)
        {
            auto* b = new juce::TextButton(t == 0 ? "LEFT" : "RIGHT");
            b->setClickingTogglesState(false);
            b->onClick = [this, t]
            {
                sideTab_ = t;
                refresh();
            };
            buttons_.add(b);
            content_.addAndMakeVisible(b);
            tab_[t] = b;
        }
        addRow({}, tab_[0]); // both tabs share one row; layoutContent splits it
        addCombo("Mode", { "Keyboard", "Arpeggiator", "Sequencer" },
                 [this](Cfg& c, int v) { scopedSide(c).mode = (s42::KbMode) v; },
                 [this](const Cfg& c) { return (int) scopedSide(c).mode; });

        addHeader("ARPEGGIATOR");
        addToggle("Hold",
                  [this](Cfg& c, bool v) { scopedSide(c).arp.hold = v; },
                  [this](const Cfg& c) { return scopedSide(c).arp.hold; });
        addCombo("Clock ratio", ratios,
                 [this](Cfg& c, int v) { scopedSide(c).arp.clockRatio = v; },
                 [this](const Cfg& c) { return scopedSide(c).arp.clockRatio; });
        addCombo("Direction", dirs,
                 [this](Cfg& c, int v) { scopedSide(c).arp.direction = (s42::KbDirection) v; },
                 [this](const Cfg& c) { return (int) scopedSide(c).arp.direction; });
        addCombo("Variation", { "Off", "x1", "x2", "x3" },
                 [this](Cfg& c, int v) { scopedSide(c).arp.variation = v; },
                 [this](const Cfg& c) { return scopedSide(c).arp.variation; });
        addSlider("Interval (semi)", 1, 12, 1,
                  [this](Cfg& c, double v) { scopedSide(c).arp.intervalSemis = (int) v; },
                  [this](const Cfg& c) { return (double) scopedSide(c).arp.intervalSemis; });
        arpRhythm_.onChange = [this](uint16_t m)
        {
            auto c = state_.config();
            scopedSide(c).arp.rhythmMask = (uint8_t) m;
            state_.set(c);
        };
        content_.addAndMakeVisible(arpRhythm_);
        addRow("Rhythm", &arpRhythm_, 26);
        addSlider("Rhythm length", 1, 8, 1,
                  [this](Cfg& c, double v) { scopedSide(c).arp.rhythmLen = (int) v; },
                  [this](const Cfg& c) { return (double) scopedSide(c).arp.rhythmLen; });

        addHeader("SEQUENCER");
        addCombo("Run", { "Free", "Keyboard controlled" },
                 [this](Cfg& c, int v) { scopedSide(c).seq.keyRun = v == 1; },
                 [this](const Cfg& c) { return scopedSide(c).seq.keyRun ? 1 : 0; });
        addSlider("Length", 2, 16, 1,
                  [this](Cfg& c, double v) { scopedSide(c).seq.length = (int) v; },
                  [this](const Cfg& c) { return (double) scopedSide(c).seq.length; });
        addCombo("Clock ratio", ratios,
                 [this](Cfg& c, int v) { scopedSide(c).seq.clockRatio = v; },
                 [this](const Cfg& c) { return scopedSide(c).seq.clockRatio; });
        addCombo("Direction", dirs,
                 [this](Cfg& c, int v) { scopedSide(c).seq.direction = (s42::KbDirection) v; },
                 [this](const Cfg& c) { return (int) scopedSide(c).seq.direction; });
        addCombo("CV output", { "Continuous", "Gated" },
                 [this](Cfg& c, int v) { scopedSide(c).seq.gatedCv = v == 1; },
                 [this](const Cfg& c) { return scopedSide(c).seq.gatedCv ? 1 : 0; });
        steps_.onValue = [this](int i, float v)
        {
            auto c = state_.config();
            scopedSide(c).seq.value[i] = v;
            state_.set(c);
        };
        steps_.onGate = [this](int i, bool g)
        {
            auto c = state_.config();
            scopedSide(c).seq.gate[i] = g;
            state_.set(c);
        };
        content_.addAndMakeVisible(steps_);
        addRow({}, &steps_, 96);
        seqRhythm_.onChange = [this](uint16_t m)
        {
            auto c = state_.config();
            scopedSide(c).seq.rhythmMask = (uint8_t) m;
            state_.set(c);
        };
        content_.addAndMakeVisible(seqRhythm_);
        addRow("Rhythm", &seqRhythm_, 26);
        addSlider("Rhythm length", 1, 8, 1,
                  [this](Cfg& c, double v) { scopedSide(c).seq.rhythmLen = (int) v; },
                  [this](const Cfg& c) { return (double) scopedSide(c).seq.rhythmLen; });

        addHeader("PRESETS A-D (all but tempo)");
        for (int i = 0; i < 4; ++i)
        {
            auto* row = new juce::Component();
            const char* names[] = { "A", "B", "C", "D" };
            auto* loadB = new juce::TextButton("Load");
            auto* saveB = new juce::TextButton("Save");
            auto* initB = new juce::TextButton("Init");
            loadB->onClick = [this, i]
            {
                auto c = state_.config();
                s42::loadPreset(c, state_.presets().slot[i]);
                state_.set(c);
            };
            saveB->onClick = [this, i]
            {
                auto p = state_.presets();
                p.slot[i] = state_.config();
                state_.setPresets(p);
            };
            initB->onClick = [this, i]
            {
                auto p = state_.presets();
                p.slot[i] = s42::KbConfig {};
                state_.setPresets(p);
            };
            for (auto* b : { loadB, saveB, initB })
            {
                buttons_.add(b);
                row->addAndMakeVisible(b);
            }
            presetRows_[i] = row;
            misc_.add(row);
            content_.addAndMakeVisible(row);
            addRow(names[i], row, 26);
        }

        // MIDI-in (M9a): this one is an APVTS param (persisted with the rig,
        // automatable), not keyboard-firmware state — hence the attachment.
        addHeader("MIDI");
        midiLatch_.setButtonText("Drone keys C1-F1 toggle HOLD");
        midiLatch_.setTooltip("Off: MIDI notes C1-F1 gate drone voices 1-6 while held "
                              "(like the DRONE VOICES keypad). On: each press flips that "
                              "voice's HOLD latch.");
        content_.addAndMakeVisible(midiLatch_);
        midiLatchAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, "midi.droneLatch", midiLatch_);
        addRow({}, &midiLatch_);

        // Content height = sum of rows + margins.
        contentHeight_ = 8;
        for (const auto& r : rows_)
            contentHeight_ += r.height + 4;
        contentHeight_ += 16;
    }

    static juce::StringArray scaleNames()
    {
        juce::StringArray a;
        for (int i = 0; i < s42::kKbNumScales; ++i)
            a.add(s42::kKbScaleNames[i]);
        return a;
    }

    void layoutContent()
    {
        const int w = content_.getWidth();
        int y = 8;
        for (auto& r : rows_)
        {
            auto area = juce::Rectangle<int>(8, y, w - 16, r.height);
            if (r.kind == RowKind::Header)
            {
                if (r.label != nullptr)
                    r.label->setBounds(area.withTrimmedTop(6));
            }
            else if (r.widget == tab_[0]) // the side-tab pair shares its row
            {
                tab_[0]->setBounds(area.removeFromLeft(area.getWidth() / 2).reduced(2, 1));
                tab_[1]->setBounds(area.reduced(2, 1));
            }
            else if (r.label != nullptr && r.widget != nullptr)
            {
                r.label->setBounds(area.removeFromLeft(w * 2 / 5));
                r.widget->setBounds(area);
            }
            else if (r.widget != nullptr)
            {
                r.widget->setBounds(area);
            }
            y += r.height + 4;
        }
        for (int i = 0; i < 4; ++i)
            if (presetRows_[i] != nullptr)
            {
                auto pr = presetRows_[i]->getLocalBounds();
                const int bw = pr.getWidth() / 3;
                presetRows_[i]->getChildComponent(0)->setBounds(pr.removeFromLeft(bw).reduced(2, 1));
                presetRows_[i]->getChildComponent(1)->setBounds(pr.removeFromLeft(bw).reduced(2, 1));
                presetRows_[i]->getChildComponent(2)->setBounds(pr.reduced(2, 1));
            }
    }

    void refresh()
    {
        loading_ = true;
        const auto c = state_.config();
        for (auto& f : refreshers_)
            f();
        scaleMask_.set(c.scaleMask, 12);
        const auto& sc = scopedSide(c);
        arpRhythm_.set(sc.arp.rhythmMask, juce::jlimit(1, 8, sc.arp.rhythmLen));
        seqRhythm_.set(sc.seq.rhythmMask, juce::jlimit(1, 8, sc.seq.rhythmLen));
        steps_.set(sc.seq);
        const bool split = c.behaviour == s42::KbBehaviour::Split;
        tab_[1]->setEnabled(split);
        if (!split)
            sideTab_ = 0;
        tab_[0]->setColour(juce::TextButton::buttonColourId,
                           sideTab_ == 0 ? juce::Colour(0xffe08a2e) : juce::Colour(0xff2f2f34));
        tab_[1]->setColour(juce::TextButton::buttonColourId,
                           split && sideTab_ == 1 ? juce::Colour(0xffe08a2e)
                                                  : juce::Colour(0xff2f2f34));
        loading_ = false;
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override { refresh(); }

    KeyboardState& state_;
    juce::AudioProcessorValueTreeState& apvts_;
    juce::Label title_;
    juce::TextButton close_;
    juce::Viewport viewport_;
    juce::Component content_;
    int contentHeight_ = 600;

    std::vector<Row> rows_;
    std::vector<std::function<void()>> refreshers_;
    juce::OwnedArray<juce::ComboBox> combos_;
    juce::OwnedArray<juce::Slider> sliders_;
    juce::OwnedArray<juce::ToggleButton> toggles_;
    juce::OwnedArray<juce::TextButton> buttons_;
    juce::OwnedArray<juce::Component> misc_;
    juce::ComboBox* scaleBox_ = nullptr;
    juce::TextButton* tab_[2] = {};
    juce::Component* presetRows_[4] = {};
    MaskEditor scaleMask_, arpRhythm_, seqRhythm_;
    StepEditor steps_;
    juce::ToggleButton midiLatch_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiLatchAtt_;
    int sideTab_ = 0;
    bool loading_ = false;
};

} // namespace solar
