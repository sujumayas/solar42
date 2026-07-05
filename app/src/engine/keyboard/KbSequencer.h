#pragma once

#include <cstdint>

#include "engine/keyboard/KbConfig.h"

// 16-step sequencer (manual p17), transposed by the active note plate.
// Directions: forward / backward / ping-pong (forward in its entirety, then
// backward in its entirety — endpoints repeat, unlike the arp) / random.
// RHYTHM gates the incoming clock exactly like the arpeggiator's.
namespace s42 {

struct KbSequencer
{
    int phase = -1;     // advances per passed clock tick; mapped to a step below
    int rhythmPos = -1;
    uint32_t rng = 0x9E3779B9u;
    int curStep = 0;

    void reset() noexcept
    {
        phase = -1;
        rhythmPos = -1;
    }

    // Called once per scaled clock tick. Returns true when the sequencer
    // advanced (rhythm let the pulse through); curStep is then valid.
    bool tick(const SeqConfig& s) noexcept
    {
        const int rlen = s.rhythmLen < 1 ? 1 : (s.rhythmLen > 8 ? 8 : s.rhythmLen);
        rhythmPos = (rhythmPos + 1) % rlen;
        if ((s.rhythmMask >> rhythmPos & 1) == 0)
            return false;

        const int len = s.length < 2 ? 2 : (s.length > 16 ? 16 : s.length);
        ++phase;
        switch (s.direction)
        {
            case KbDirection::Forward:
                phase %= len;
                curStep = phase;
                break;
            case KbDirection::Backward:
                phase %= len;
                curStep = len - 1 - phase;
                break;
            case KbDirection::PingPong: // 0,1..N-1,N-1..1,0 then repeat
                phase %= 2 * len;
                curStep = phase < len ? phase : 2 * len - 1 - phase;
                break;
            case KbDirection::Random:
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                curStep = (int) (rng % (uint32_t) len);
                break;
        }
        return true;
    }
};

} // namespace s42
