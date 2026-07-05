#include "engine/keyboard/KbMenu.h"

#include "engine/keyboard/Quantiser.h"

#include <cstdio>
#include <cstring>

namespace s42 {

namespace
{
const char* const kItemNames[] = {
    "BEHAVIOUR", "SIDE", "MODE",
    "ARP HOLD", "ARP CLOCK", "ARP DIRECT.", "ARP VARIAT.", "ARP INTERVAL", "ARP RHYTHM", "ARP RHY.LEN",
    "SEQ RUN", "SEQ LENGTH", "SEQ CLOCK", "SEQ DIRECT.", "SEQ EDITOR", "SEQ CV OUT", "SEQ RHYTHM", "SEQ RHY.LEN",
    "PORTAMENTO", "LEGATO",
    "VIB SPEED", "VIB DEPTH", "VIB DELAY", "VIB PRESS.",
    "PRESS. OUT", "PRESS. RISE", "PRESS. FALL",
    "SCALE EDIT", "LOAD SCALE", "ROOT NOTE",
    "CLOCK BPM",
    "PRESETS",
};

const char* const kBehaviourNames[] = { "SINGLE", "TWIN", "SPLIT" };
const char* const kModeNames[] = { "KEYBOARD", "ARPEGGIO", "SEQUENCER" };
const char* const kDirNames[] = { "FORWARD", "BACKWARD", "PING-PONG", "RANDOM" };
const char* const kVarNames[] = { "OFF", "X1", "X2", "X3" };
const char* const kRunNames[] = { "FREE", "KEYBOARD" };
const char* const kCvNames[] = { "CONTINUOUS", "GATED" };
const char* const kPressNames[] = { "PRESSURE", "ASR", "AD", "LOOP AD", "RANDOM" };
const char* const kPresetNames[] = { "A", "B", "C", "D" };
const char* const kPresetActions[] = { "LOAD", "SAVE", "INIT" };

int clampi(int v, int lo, int hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

void rhythmString(char* out, size_t cap, uint8_t mask, int len, int cursor) noexcept
{
    size_t w = 0;
    for (int i = 0; i < len && w + 2 < cap; ++i)
    {
        out[w++] = (char) (i == cursor ? '[' : ' ');
        out[w++] = (mask >> i & 1) != 0 ? 'X' : '.';
        if (i == cursor && w < cap)
            out[w++] = ']';
    }
    out[w < cap ? w : cap - 1] = '\0';
}
} // namespace

bool KbMenu::itemVisible(int item, const KbConfig& c) const noexcept
{
    if (item == SideSel)
        return c.behaviour == KbBehaviour::Split;
    return true;
}

void KbMenu::rotate(int detents, KbConfig& c, KbPresets& p) noexcept
{
    (void) p;
    switch (page_)
    {
        case Page::Browse:
        {
            int it = item_;
            const int dir = detents >= 0 ? 1 : -1;
            for (int move = detents * dir; move > 0; --move)
            {
                do
                    it = (it + dir + ItemCount) % ItemCount;
                while (!itemVisible(it, c));
            }
            item_ = it;
            break;
        }
        case Page::Edit:
            adjust(detents, c);
            break;
        case Page::ScaleEdit:
            cursor_ = clampi(cursor_ + detents, 0, 11);
            break;
        case Page::RhythmEdit:
        {
            auto& sc = sideOf(c);
            const int len = rhythmIsSeq_ ? sc.seq.rhythmLen : sc.arp.rhythmLen;
            cursor_ = clampi(cursor_ + detents, 0, clampi(len, 1, 8) - 1);
            break;
        }
        case Page::SeqEdit:
        {
            // Slots alternate gate/value per step (manual p17: the editor
            // walks both the gates and the note/value faders).
            auto& sq = sideOf(c).seq;
            if (seqValueEdit_)
            {
                const int step = cursor_ / 2;
                float v = sq.value[step] + (float) detents
                          * (c.scaleMask != 0 ? 1.0f / 12.0f : kKbMicroStepVolts);
                sq.value[step] = v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v);
            }
            else
                cursor_ = clampi(cursor_ + detents, 0, 2 * sq.length - 1);
            break;
        }
        case Page::PresetSel:
            presetSel_ = clampi(presetSel_ + detents, 0, 3);
            break;
        case Page::PresetAct:
            presetAction_ = clampi(presetAction_ + detents, 0, 2);
            break;
    }
}

void KbMenu::adjust(int d, KbConfig& c) noexcept
{
    auto& sc = sideOf(c);
    switch (item_)
    {
        case Behaviour: c.behaviour = (KbBehaviour) clampi((int) c.behaviour + d, 0, 2); break;
        case SideSel:   side_ = clampi(side_ + d, 0, 1); break;
        case Mode:      sc.mode = (KbMode) clampi((int) sc.mode + d, 0, 2); break;

        case ArpHold:     sc.arp.hold = d > 0; break;
        case ArpClock:    sc.arp.clockRatio = clampi(sc.arp.clockRatio + d, 0, kKbNumClockRatios - 1); break;
        case ArpDir:      sc.arp.direction = (KbDirection) clampi((int) sc.arp.direction + d, 0, 3); break;
        case ArpVar:      sc.arp.variation = clampi(sc.arp.variation + d, 0, 3); break;
        case ArpInterval: sc.arp.intervalSemis = clampi(sc.arp.intervalSemis + d, 1, 12); break;
        case ArpRhythmLen: sc.arp.rhythmLen = clampi(sc.arp.rhythmLen + d, 1, 8); break;

        case SeqRun:    sc.seq.keyRun = d > 0; break;
        case SeqLength: sc.seq.length = clampi(sc.seq.length + d, 2, 16); break;
        case SeqClock:  sc.seq.clockRatio = clampi(sc.seq.clockRatio + d, 0, kKbNumClockRatios - 1); break;
        case SeqDir:    sc.seq.direction = (KbDirection) clampi((int) sc.seq.direction + d, 0, 3); break;
        case SeqCvMode: sc.seq.gatedCv = d > 0; break;
        case SeqRhythmLen: sc.seq.rhythmLen = clampi(sc.seq.rhythmLen + d, 1, 8); break;

        case Portamento: c.portamento = clampi(c.portamento + d, 0, 255); break;
        case Legato:     c.legato = d > 0; break;

        case VibSpeed: c.vibSpeed = clampi(c.vibSpeed + d, 0, 127); break;
        case VibDepth: c.vibDepth = clampi(c.vibDepth + d, 0, 127); break;
        case VibDelay: c.vibDelay = clampi(c.vibDelay + d, 0, 127); break;
        case VibPress: c.vibPressure = d > 0; break;

        case PressOut:  c.pressureMode = (KbPressureMode) clampi((int) c.pressureMode + d, 0, 4); break;
        case PressRise: c.rise = clampi(c.rise + d, 0, 255); break;
        case PressFall: c.fall = clampi(c.fall + d, 0, 255); break;

        case LoadScale: scaleSel_ = clampi(scaleSel_ + d, 0, kKbNumScales - 1); break;
        case RootNote:  c.rootNote = clampi(c.rootNote + d, 0, 11); break;

        case ClockBpm:
            c.bpm = (float) clampi((int) (c.bpm + 0.5f) + d, 10, 300);
            ++c.bpmEdit; // documented way back to the internal clock
            break;

        default: break;
    }
}

void KbMenu::click(KbConfig& c, KbPresets& p) noexcept
{
    switch (page_)
    {
        case Page::Browse:
            switch (item_)
            {
                case ArpRhythm:
                case SeqRhythm:
                    rhythmIsSeq_ = item_ == SeqRhythm;
                    cursor_ = 0;
                    page_ = Page::RhythmEdit;
                    break;
                case SeqEditor:
                    cursor_ = 0;
                    seqValueEdit_ = false;
                    page_ = Page::SeqEdit;
                    break;
                case ScaleEditor:
                    cursor_ = 0;
                    page_ = Page::ScaleEdit;
                    break;
                case Presets:
                    presetSel_ = 0;
                    page_ = Page::PresetSel;
                    break;
                default:
                    page_ = Page::Edit;
                    break;
            }
            break;

        case Page::Edit:
            if (item_ == LoadScale) // confirming loads scale + retunes plates
                kbLoadScale(c, scaleSel_);
            page_ = Page::Browse;
            break;

        case Page::ScaleEdit:
            c.scaleMask = (uint16_t) (c.scaleMask ^ (1u << cursor_));
            break;

        case Page::RhythmEdit:
        {
            auto& sc = sideOf(c);
            uint8_t& mask = rhythmIsSeq_ ? sc.seq.rhythmMask : sc.arp.rhythmMask;
            mask = (uint8_t) (mask ^ (1u << cursor_));
            break;
        }

        case Page::SeqEdit:
        {
            auto& sq = sideOf(c).seq;
            if (cursor_ % 2 == 0) // gate slot: click toggles
                sq.gate[cursor_ / 2] = !sq.gate[cursor_ / 2];
            else                  // value slot: click highlights for editing
                seqValueEdit_ = !seqValueEdit_;
            break;
        }

        case Page::PresetSel:
            presetAction_ = 0;
            page_ = Page::PresetAct;
            break;

        case Page::PresetAct:
            switch (presetAction_)
            {
                case 0: loadPreset(c, p.slot[presetSel_]); break;
                case 1: p.slot[presetSel_] = c; break;
                case 2: p.slot[presetSel_] = KbConfig {}; break;
            }
            page_ = Page::Browse;
            break;
    }
}

void KbMenu::longPress(KbConfig& c, KbPresets& p) noexcept
{
    (void) c;
    (void) p;
    seqValueEdit_ = false;
    page_ = Page::Browse; // manual: hold the encoder 1 s to leave an editor
}

KbMenu::Display KbMenu::display(const KbConfig& c) const noexcept
{
    Display d;
    const auto& sc = sideOf(c);
    auto set = [&d](const char* l1, const char* l2)
    {
        std::snprintf(d.line1, sizeof(d.line1), "%s", l1);
        std::snprintf(d.line2, sizeof(d.line2), "%s", l2);
    };
    char buf[24];

    switch (page_)
    {
        case Page::Browse:
        case Page::Edit:
        {
            d.editing = page_ == Page::Edit;
            const char* value = buf;
            switch (item_)
            {
                case Behaviour: value = kBehaviourNames[(int) c.behaviour]; break;
                case SideSel:   value = side_ == 0 ? "LEFT" : "RIGHT"; break;
                case Mode:      value = kModeNames[(int) sc.mode]; break;
                case ArpHold:   value = sc.arp.hold ? "ON" : "OFF"; break;
                case ArpClock:  value = kKbClockRatioNames[sc.arp.clockRatio]; break;
                case ArpDir:    value = kDirNames[(int) sc.arp.direction]; break;
                case ArpVar:    value = kVarNames[sc.arp.variation]; break;
                case ArpInterval: std::snprintf(buf, sizeof(buf), "%d SEMI", sc.arp.intervalSemis); break;
                case ArpRhythm: rhythmString(buf, sizeof(buf), sc.arp.rhythmMask, sc.arp.rhythmLen, -1); break;
                case ArpRhythmLen: std::snprintf(buf, sizeof(buf), "%d", sc.arp.rhythmLen); break;
                case SeqRun:    value = kRunNames[sc.seq.keyRun ? 1 : 0]; break;
                case SeqLength: std::snprintf(buf, sizeof(buf), "%d", sc.seq.length); break;
                case SeqClock:  value = kKbClockRatioNames[sc.seq.clockRatio]; break;
                case SeqDir:    value = kDirNames[(int) sc.seq.direction]; break;
                case SeqEditor: value = "[CLICK]"; break;
                case SeqCvMode: value = kCvNames[sc.seq.gatedCv ? 1 : 0]; break;
                case SeqRhythm: rhythmString(buf, sizeof(buf), sc.seq.rhythmMask, sc.seq.rhythmLen, -1); break;
                case SeqRhythmLen: std::snprintf(buf, sizeof(buf), "%d", sc.seq.rhythmLen); break;
                case Portamento: std::snprintf(buf, sizeof(buf), "%d", c.portamento); break;
                case Legato:    value = c.legato ? "ON" : "OFF"; break;
                case VibSpeed:  std::snprintf(buf, sizeof(buf), "%d", c.vibSpeed); break;
                case VibDepth:  std::snprintf(buf, sizeof(buf), "%d", c.vibDepth); break;
                case VibDelay:  std::snprintf(buf, sizeof(buf), "%d", c.vibDelay); break;
                case VibPress:  value = c.vibPressure ? "ON" : "OFF"; break;
                case PressOut:  value = kPressNames[(int) c.pressureMode]; break;
                case PressRise: std::snprintf(buf, sizeof(buf), "%d", c.rise); break;
                case PressFall: std::snprintf(buf, sizeof(buf), "%d", c.fall); break;
                case ScaleEditor: value = c.scaleMask == 0 ? "MICROTONAL" : "[CLICK]"; break;
                case LoadScale: value = kKbScaleNames[scaleSel_]; break;
                case RootNote:  value = kKbRootNames[c.rootNote]; break;
                case ClockBpm:
                    std::snprintf(buf, sizeof(buf), "%d BPM", (int) (c.bpm + 0.5f));
                    break;
                case Presets:   value = "[CLICK]"; break;
                default:        buf[0] = '\0'; break;
            }
            set(kItemNames[item_], value);
            break;
        }

        case Page::ScaleEdit:
        {
            char row[24];
            size_t w = 0;
            for (int i = 0; i < 12 && w + 1 < sizeof(row); ++i)
                row[w++] = (c.scaleMask >> i & 1) != 0 ? 'X' : '.';
            row[w] = '\0';
            std::snprintf(buf, sizeof(buf), "NOTE %s %s", kKbRootNames[(c.rootNote + cursor_) % 12],
                          (c.scaleMask >> cursor_ & 1) != 0 ? "ON" : "OFF");
            set(buf, row);
            d.editing = true;
            d.cursor = cursor_;
            break;
        }

        case Page::RhythmEdit:
        {
            const uint8_t mask = rhythmIsSeq_ ? sc.seq.rhythmMask : sc.arp.rhythmMask;
            const int len = rhythmIsSeq_ ? sc.seq.rhythmLen : sc.arp.rhythmLen;
            rhythmString(buf, sizeof(buf), mask, len, cursor_);
            set(rhythmIsSeq_ ? "SEQ RHYTHM" : "ARP RHYTHM", buf);
            d.editing = true;
            d.cursor = cursor_;
            break;
        }

        case Page::SeqEdit:
        {
            const auto& sq = sc.seq;
            const int step = cursor_ / 2;
            std::snprintf(buf, sizeof(buf), "STEP %d %s", step + 1,
                          cursor_ % 2 == 0 ? "GATE" : "VALUE");
            char val[24];
            if (cursor_ % 2 == 0)
                std::snprintf(val, sizeof(val), "%s", sq.gate[step] ? "ON" : "OFF");
            else
                std::snprintf(val, sizeof(val), "%s%.3f V%s", seqValueEdit_ ? ">" : " ",
                              (double) sq.value[step], seqValueEdit_ ? "<" : " ");
            set(buf, val);
            d.editing = seqValueEdit_;
            d.cursor = step;
            break;
        }

        case Page::PresetSel:
            std::snprintf(buf, sizeof(buf), "PRESET %s", kPresetNames[presetSel_]);
            set(buf, "[CLICK]");
            d.cursor = presetSel_;
            break;

        case Page::PresetAct:
            std::snprintf(buf, sizeof(buf), "PRESET %s", kPresetNames[presetSel_]);
            set(buf, kPresetActions[presetAction_]);
            d.editing = true;
            break;
    }
    return d;
}

} // namespace s42
