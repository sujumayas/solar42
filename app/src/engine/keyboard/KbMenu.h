#pragma once

#include "engine/keyboard/KbConfig.h"

// The keyboard's push-encoder menu as a pure state machine (manual pp15-19):
// rotate scrolls the parameter list, click selects/deselects a parameter for
// editing, rotating while editing changes the value. RHYTHM, the SCALE
// editor, the SEQ step editor and the PRESET page are sub-pages entered by
// clicking; a 1-second long press leaves a sub-page (manual: seq editor).
// The UI renders Display's two lines on the little screen and forwards
// encoder gestures; the settings drawer edits the same KbConfig directly.
namespace s42 {

class KbMenu
{
public:
    struct Display
    {
        char line1[24] = {};
        char line2[24] = {};
        bool editing = false; // highlight the value line
        int cursor = -1;      // sub-editor position (plate/step/note), -1 = none
    };

    void rotate(int detents, KbConfig& c, KbPresets& p) noexcept;
    void click(KbConfig& c, KbPresets& p) noexcept;
    void longPress(KbConfig& c, KbPresets& p) noexcept;
    Display display(const KbConfig& c) const noexcept;

    int activeSide() const noexcept { return side_; }

private:
    enum class Page : uint8_t { Browse, Edit, ScaleEdit, RhythmEdit, SeqEdit, PresetSel, PresetAct };

    enum Item : int
    {
        Behaviour, SideSel, Mode,
        ArpHold, ArpClock, ArpDir, ArpVar, ArpInterval, ArpRhythm, ArpRhythmLen,
        SeqRun, SeqLength, SeqClock, SeqDir, SeqEditor, SeqCvMode, SeqRhythm, SeqRhythmLen,
        Portamento, Legato,
        VibSpeed, VibDepth, VibDelay, VibPress,
        PressOut, PressRise, PressFall,
        ScaleEditor, LoadScale, RootNote,
        ClockBpm,
        Presets,
        ItemCount
    };

    bool itemVisible(int item, const KbConfig& c) const noexcept;
    void adjust(int detents, KbConfig& c) noexcept; // value edit on the current item
    KbSideConfig& sideOf(KbConfig& c) const noexcept
    {
        return c.side[c.behaviour == KbBehaviour::Split ? side_ : 0];
    }
    const KbSideConfig& sideOf(const KbConfig& c) const noexcept
    {
        return c.side[c.behaviour == KbBehaviour::Split ? side_ : 0];
    }

    Page page_ = Page::Browse;
    int item_ = Behaviour;
    int side_ = 0;         // split: which side the arp/seq/mode items edit
    int cursor_ = 0;       // sub-editor position
    bool seqValueEdit_ = false;
    bool rhythmIsSeq_ = false;
    int scaleSel_ = 0;     // LOAD SCALE pending selection
    int presetSel_ = 0, presetAction_ = 0;
};

} // namespace s42
