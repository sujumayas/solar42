#pragma once

// Every value the manuals do NOT specify lives here and nowhere else
// (07-42n-panel-inventory.md §6). Each constant carries the reasoning for its
// initial estimate; the M8 calibration-by-ear pass against SOUND DEMO 3 and
// hardware videos edits THIS FILE ONLY.
//
// Convention: *_Min/_Max are knob-range endpoints; times in seconds, freqs in Hz.

namespace s42::tuning {

// ---- Drone voice AR envelope (manual gives no times; hardware tails are long)
inline constexpr float kDroneAttMin = 0.001f;
inline constexpr float kDroneAttMax = 10.0f;   // slow swell heard in demos
inline constexpr float kDroneRlsMin = 0.005f;
inline constexpr float kDroneRlsMax = 20.0f;   // "endless" fade-outs

// ---- VCO ADSR (typical analog EG ranges; LOOP usable as LFO at short A/R)
inline constexpr float kVcoAttMin = 0.001f;
inline constexpr float kVcoAttMax = 8.0f;
inline constexpr float kVcoDecMin = 0.002f;
inline constexpr float kVcoDecMax = 12.0f;
inline constexpr float kVcoRelMin = 0.002f;
inline constexpr float kVcoRelMax = 15.0f;

// RC envelope attack charges toward an overshoot target then clips -> analog snap.
inline constexpr float kEnvAttackOvershoot = 1.30f;

// ---- LFOs (manual: A "slow", B "fast", x1/x6/x10 range switch; no Hz given)
inline constexpr float kLfoAMin = 0.02f, kLfoAMax = 5.0f;
inline constexpr float kLfoBMin = 0.2f,  kLfoBMax = 50.0f;

// ---- 5-step sequencer pulser (internal clock; no range given)
inline constexpr float kPulserMin = 0.1f, kPulserMax = 20.0f;

// ---- Polivoks filter (range/self-osc unspecified; "dirty and angry", keeps bass)
inline constexpr float kFilterFreqMin = 16.0f;
inline constexpr float kFilterFreqMax = 18000.0f;
inline constexpr float kFilterSelfOscRes = 0.85f; // resonance knob position where it starts to sing
inline constexpr float kFilterLrSkew = 0.025f;    // +-2.5 % L/R component tolerance

// ---- Classic drone voice VOLT knob (transpose depth + dirty zone unspecified)
inline constexpr float kVoltTransposeOct = 2.5f;  // full downward transpose span
inline constexpr float kVoltDirtyStart = 0.5f;    // knob travel where cross-FM begins
inline constexpr float kVoltCrossFmMax = 0.35f;   // cross-mod index at full travel

// ---- Photo sensor (LDR/vactrol behavior; no numbers in manual)
inline constexpr float kSensorAttack = 0.005f;    // light up: fast
inline constexpr float kSensorDecay = 0.120f;     // light down: LDR memory
inline constexpr float kSensorModDepthOct = 0.6f; // full-LED pitch push on a generator

// ---- Papa Srapa (rate range / divider ratios / x10 factor unspecified)
inline constexpr float kSrapaRateMin = 0.08f, kSrapaRateMax = 12.0f; // x1 position
inline constexpr float kSrapaRateX10 = 10.0f;
inline constexpr int   kSrapaDividers[] = { 1, 2, 4, 8, 16 };

// ---- Keyboard slews (0-255 / 0-127 firmware units -> seconds, unspecified)
inline constexpr float kPortamentoMaxSec = 2.5f;  // at 255
inline constexpr float kPressureSlewMaxSec = 4.0f;// rise/fall at 255
inline constexpr float kVibratoMaxHz = 12.0f;     // speed 127

} // namespace s42::tuning
