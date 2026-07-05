#pragma once

#include <cmath>

#include "engine/keyboard/KbConfig.h"

// Note quantiser (manual p18): a pitch-class mask + root note snap the plate
// (or plate+step) voltage to the nearest enabled semitone. All notes off =
// microtonal pass-through. LOAD SCALE also rewrites the plates so the 12
// plates walk the scale's degrees ascending from the root ("load a preset
// scale to the quantiser and to the keyboard also").
namespace s42 {

// The manual lists 19 preset scales by name only — the exact interval sets
// are not printed. Masks below use the standard definitions of those names;
// bit k = interval k semitones above the root. Whatever the real firmware
// ships, the user can reach any set through the scale editor.
inline constexpr const char* kKbScaleNames[] = {
    "Semitones", "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian",
    "Aeolian", "Locrian", "Blues major", "Blues minor", "Pentatonic major",
    "Pentatonic minor", "Folk", "Japanese", "Gamelan", "Gypsy", "Arabian",
    "Flamenco", "Whole tone"
};

inline constexpr uint16_t kKbScaleMasks[] = {
    0x0FFF, // Semitones      C C# D D# E F F# G G# A A# B
    0x0AB5, // Ionian         0 2 4 5 7 9 11
    0x06AD, // Dorian         0 2 3 5 7 9 10
    0x05AB, // Phrygian       0 1 3 5 7 8 10
    0x0AD5, // Lydian         0 2 4 6 7 9 11
    0x06B5, // Mixolydian     0 2 4 5 7 9 10
    0x05AD, // Aeolian        0 2 3 5 7 8 10
    0x056B, // Locrian        0 1 3 5 6 8 10
    0x029D, // Blues major    0 2 3 4 7 9
    0x04E9, // Blues minor    0 3 5 6 7 10
    0x0295, // Pentatonic maj 0 2 4 7 9
    0x04A9, // Pentatonic min 0 3 5 7 10
    0x05BB, // Folk           0 1 3 4 5 7 8 10
    0x01A3, // Japanese       0 1 5 7 8
    0x018B, // Gamelan        0 1 3 7 8
    0x09CD, // Gypsy          0 2 3 6 7 8 11
    0x09B3, // Arabian        0 1 4 5 7 8 11
    0x05B3, // Flamenco       0 1 4 5 7 8 10
    0x0555, // Whole tone     0 2 4 6 8 10
};

inline constexpr int kKbNumScales = 19;

// German note naming, as the manual specifies root "from C to H".
inline constexpr const char* kKbRootNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "B", "H"
};

// Snap volts to the nearest enabled semitone (ties resolve downward).
// mask == 0 -> microtonal keyboard, value passes through untouched.
inline float kbQuantise(float volts, uint16_t mask, int root) noexcept
{
    if (mask == 0)
        return volts;

    const float semis = volts * 12.0f;
    const int centre = (int) std::lround(semis);
    int best = centre;
    float bestDist = 1.0e9f;
    for (int s = centre - 6; s <= centre + 6; ++s) // 13 semis span every pitch class
    {
        const int pc = ((s - root) % 12 + 12) % 12;
        if ((mask >> pc & 1) == 0)
            continue;
        const float d = std::abs((float) s - semis);
        if (d < bestDist - 1.0e-6f)
        {
            bestDist = d;
            best = s;
        }
    }
    return (float) best / 12.0f;
}

// LOAD SCALE: set the quantiser mask and retune the 12 plates to successive
// scale degrees ascending from the root.
inline void kbLoadScale(KbConfig& c, int scaleIdx) noexcept
{
    if (scaleIdx < 0 || scaleIdx >= kKbNumScales)
        return;
    const uint16_t mask = kKbScaleMasks[scaleIdx];
    c.scaleMask = mask;

    int placed = 0;
    for (int s = 0; placed < 12 && s < 12 * 11; ++s)
        if ((mask >> (s % 12) & 1) != 0)
            c.plateVolts[placed++] = kKbPlateBaseVolts + (float) (c.rootNote + s) / 12.0f;
}

// Per-plate tuning (press a plate + rotate the encoder): semitone steps when
// the quantiser is on, 0.0025 V steps when microtonal.
inline void kbTunePlate(KbConfig& c, int plate, int detents) noexcept
{
    if (plate < 0 || plate >= 12)
        return;
    const float step = c.scaleMask != 0 ? 1.0f / 12.0f : kKbMicroStepVolts;
    float v = c.plateVolts[plate] + step * (float) detents;
    c.plateVolts[plate] = v < 0.0f ? 0.0f : (v > 8.0f ? 8.0f : v);
}

} // namespace s42
