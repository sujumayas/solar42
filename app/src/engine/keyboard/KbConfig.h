#pragma once

#include <cstdint>

// Touch keyboard firmware data model (manual pp13-20). Everything the
// encoder menu, the settings drawer and presets A-D operate on lives in
// KbConfig — one POD struct so the message thread can hand the audio thread
// a consistent snapshot (SeqlockBox) and presets are plain copies.
//
// Semantics recap (solar42-manual.txt, SENSOR KEYBOARD chapters):
// - Behaviour: single (12 plates) / twin (2x6, PRESSURE jack becomes the
//   right side's V/oct, params shared) / split (like twin, separate params).
// - Mode per side: keyboard / arpeggiator / 16-step sequencer.
// - Quantiser: pitch-class mask + root; all notes off = microtonal keyboard
//   (plates hand-tunable in 0.0025 V steps).
// - Clock: internal 10-300 BPM; auto-switches to external on a CLOCK-in
//   pulse; editing the BPM reverts to internal (bpmEdit counts edits).
// - Presets A-D store every parameter except the clock tempo.
namespace s42 {

enum class KbBehaviour : uint8_t { Single, Twin, Split };
enum class KbMode : uint8_t { Keyboard, Arp, Seq };
enum class KbDirection : uint8_t { Forward, Backward, PingPong, Random };
enum class KbPressureMode : uint8_t { Pressure, Asr, Ad, LoopAd, Random };

// Clock multiplication/division ratios for arp and sequencer (the manual
// names the parameter but not the ratio set — a firmware-detail estimate).
inline constexpr float kKbClockRatios[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
inline constexpr const char* kKbClockRatioNames[] = { "/8", "/4", "/2", "x1", "x2", "x4", "x8" };
inline constexpr int kKbNumClockRatios = 7;
inline constexpr int kKbClockUnity = 3;

// Plates' designated values default to C..B around the middle of the 0-8 V
// range (the manual does not give the factory octave).
inline constexpr float kKbPlateBaseVolts = 3.0f;
inline constexpr float kKbMicroStepVolts = 0.0025f; // manual: microtonal tuning resolution

struct ArpConfig
{
    bool hold = false;
    int clockRatio = kKbClockUnity;                  // index into kKbClockRatios
    KbDirection direction = KbDirection::Forward;
    int variation = 0;                               // 0 = off, 1..3 repeats
    int intervalSemis = 12;                          // variation transpose, 1..12
    uint8_t rhythmMask = 0x01;                       // gate seq on the clock, bit = step
    int rhythmLen = 1;                               // 1..8 (len 1 + bit set = pass-through)
};

struct SeqConfig
{
    bool keyRun = false;                             // false = free, true = advances while a plate is held
    int length = 8;                                  // 2..16
    int clockRatio = kKbClockUnity;
    KbDirection direction = KbDirection::Forward;
    float value[16] = {};                            // per-step volts, 0..2 (2-octave fader span)
    bool gate[16] = { true, true, true, true, true, true, true, true,
                      true, true, true, true, true, true, true, true };
    bool gatedCv = false;                            // true = CV updates only on gated steps
    uint8_t rhythmMask = 0x01;
    int rhythmLen = 1;
};

struct KbSideConfig
{
    KbMode mode = KbMode::Keyboard;
    ArpConfig arp {};
    SeqConfig seq {};
};

struct KbConfig
{
    KbBehaviour behaviour = KbBehaviour::Single;
    KbSideConfig side[2] {};   // single/twin read side[0]; split reads both

    // Quantiser. scaleMask bit k = pitch class (root + k) % 12 enabled;
    // mask 0 = microtonal (quantiser off).
    uint16_t scaleMask = 0x0FFF; // factory: Semitones
    int rootNote = 0;            // 0..11, C..H
    float plateVolts[12] = {
        kKbPlateBaseVolts + 0.0f / 12.0f, kKbPlateBaseVolts + 1.0f / 12.0f,
        kKbPlateBaseVolts + 2.0f / 12.0f, kKbPlateBaseVolts + 3.0f / 12.0f,
        kKbPlateBaseVolts + 4.0f / 12.0f, kKbPlateBaseVolts + 5.0f / 12.0f,
        kKbPlateBaseVolts + 6.0f / 12.0f, kKbPlateBaseVolts + 7.0f / 12.0f,
        kKbPlateBaseVolts + 8.0f / 12.0f, kKbPlateBaseVolts + 9.0f / 12.0f,
        kKbPlateBaseVolts + 10.0f / 12.0f, kKbPlateBaseVolts + 11.0f / 12.0f
    };

    // Transpose-button offsets, signed volts (single + quantiser: buttons
    // shift octaves instead; twin/split: button b toggles side b's offset).
    // Manual: +-5 octaves, semitone steps (quantised) / 0.0025 V (microtonal).
    float offsetVolts[2] = { -1.0f, 1.0f };

    int portamento = 0;          // 0..255 glide amount
    bool legato = false;         // glide only when 2+ plates touched (keyboard mode)

    int vibSpeed = 32, vibDepth = 0, vibDelay = 0; // 0..127
    bool vibPressure = false;    // pressure scales the vibrato depth

    KbPressureMode pressureMode = KbPressureMode::Pressure;
    int rise = 0, fall = 0;      // 0..255 slew / envelope times

    float bpm = 120.0f;          // internal clock, 10..300
    uint32_t bpmEdit = 0;        // bumped on every manual BPM edit -> revert to internal clock
};

// UI -> engine performance gestures (rides Rack::Controls).
struct KbTouch
{
    float plate[12] = {};        // capacitive pressure 0..1 per plate
    bool button[2] = {};         // transpose pushbuttons
};

// Presets A-D (message-thread only; persisted in the KEYBOARD state tree).
struct KbPresets
{
    KbConfig slot[4] {};
};

// Loading a preset keeps the live tempo (manual: presets store everything
// except the clock tempo).
inline void loadPreset(KbConfig& c, const KbConfig& preset) noexcept
{
    const float bpm = c.bpm;
    const uint32_t edits = c.bpmEdit;
    c = preset;
    c.bpm = bpm;
    c.bpmEdit = edits;
}

} // namespace s42
