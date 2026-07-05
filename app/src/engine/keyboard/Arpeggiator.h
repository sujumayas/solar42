#pragma once

#include <cstdint>

#include "engine/keyboard/KbConfig.h"

// Arpeggiator (manual p16). Unlike piano-key arps, the Solar builds the
// arpeggio from the *plate numbers* of the pressed plates (ascending) plus
// the direction parameter. VARIATION replays the whole progression
// transposed by INTERVAL semitones, up to 3 extra passes. RHYTHM is a 1-8
// step gate sequencer sitting between the clock and the arp: disabled steps
// mute (swallow) the clock pulse. RESET-in returns to the first step.
namespace s42 {

struct Arpeggiator
{
    int pos = 0;        // index within the current directional cycle
    int pass = 0;       // variation pass, 0..variation
    int rhythmPos = -1; // advances on every incoming (scaled) clock tick
    uint32_t rng = 0x2545F491u;

    int curPlate = -1;         // local plate index of the sounding note
    int curTransposeSemis = 0; // variation transpose of the sounding note

    void reset() noexcept
    {
        pos = 0;
        pass = 0;
        rhythmPos = -1;
    }

    // Called once per scaled clock tick with the latched/held chord (local
    // plate bits). Returns true when a new note fires (rhythm let the pulse
    // through and the chord is non-empty).
    bool tick(uint16_t chord, const ArpConfig& a) noexcept
    {
        const int rlen = a.rhythmLen < 1 ? 1 : (a.rhythmLen > 8 ? 8 : a.rhythmLen);
        rhythmPos = (rhythmPos + 1) % rlen;
        if ((a.rhythmMask >> rhythmPos & 1) == 0)
            return false;

        int notes[12];
        int k = 0;
        for (int p = 0; p < 12; ++p)
            if ((chord >> p & 1) != 0)
                notes[k++] = p;
        if (k == 0)
            return false;

        const bool pingPong = a.direction == KbDirection::PingPong && k > 1;
        const int cycle = pingPong ? 2 * k - 2 : k;
        if (pos >= cycle) // chord shrank between ticks
            pos = 0;
        const int passes = 1 + (a.variation < 0 ? 0 : (a.variation > 3 ? 3 : a.variation));
        if (pass >= passes) // variation was dialed down mid-run
            pass = 0;

        int noteIdx = 0;
        switch (a.direction)
        {
            case KbDirection::Forward:  noteIdx = pos; break;
            case KbDirection::Backward: noteIdx = k - 1 - pos; break;
            case KbDirection::PingPong: noteIdx = pos < k ? pos : 2 * k - 2 - pos; break;
            case KbDirection::Random:
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                noteIdx = (int) (rng % (uint32_t) k);
                break;
        }
        curPlate = notes[noteIdx];
        curTransposeSemis = pass * a.intervalSemis;

        if (++pos >= cycle)
        {
            pos = 0;
            pass = (pass + 1) % passes;
        }
        return true;
    }
};

} // namespace s42
