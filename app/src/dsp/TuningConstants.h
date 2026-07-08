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
inline constexpr float kFilterNegDamp = 0.25f;    // damping at RES=1 (negative => bounded self-osc growl)
inline constexpr float kFilterDrive = 1.4f;       // input gain into the asymmetric drive clip
inline constexpr float kFilterDrivePosV = 6.0f;   // drive clip knee, positive half (volts)
inline constexpr float kFilterDriveNegV = 9.0f;   // drive clip knee, negative half (asymmetry = even harmonics)
inline constexpr float kFilterFbClipV = 8.0f;     // hard-ish saturator level in the resonance path
inline constexpr float kFilterModOctPerVolt = 1.0f; // cutoff CV law at MOD knob full (1 V/oct feels right)

// ---- Post-filter "double distortion" (DIST = clean/dirty balance, GAIN = amount)
inline constexpr float kDistDriveMax = 24.0f;     // drive multiplier at GAIN=1 (two cascaded stages)
inline constexpr float kDistStage1PosV = 4.0f, kDistStage1NegV = 6.5f; // stage knees, volts
inline constexpr float kDistStage2PosV = 3.0f, kDistStage2NegV = 5.0f;
inline constexpr float kDistMakeup = 0.5f;        // dirty-path level so DIST sweeps stay ~equal-loud

// ---- Mixer (passive-feel bus: 10 channels at full would slam the rails)
inline constexpr float kMixerPostGain = 0.5f;     // bus trim into the filters (one full voice ≈ M1/M2 level)
inline constexpr float kExtAudioVolts = 5.0f;     // EXT AUDIO line in: +-1 fs -> +-5 V

// ---- Classic drone voice VOLT knob (transpose depth + dirty zone unspecified)
inline constexpr float kVoltTransposeOct = 2.5f;  // full downward transpose span
inline constexpr float kVoltDirtyStart = 0.5f;    // knob travel where cross-FM begins
inline constexpr float kVoltCrossFmMax = 0.35f;   // cross-mod index at full travel

// ---- Classic drone generators
inline constexpr float kGenToleranceCents = 8.0f; // fixed per-unit detune spread
inline constexpr float kGenLevelVolts = 2.0f;     // one generator's peak, volts
inline constexpr float kDroneCvOctPerVolt = 0.08f;// CV-jack pitch-mod depth (free-run gens, no V/oct law)

// ---- Photo sensor (LDR/vactrol behavior; no numbers in manual)
// The voice CV jack drives the sensor's red LED; the LDR adds room light and
// lags asymmetrically (vactrol memory). Output is CV-bus volts 0..10, so the
// full-LED pitch push = 10 V * kDroneCvOctPerVolt = 0.8 oct.
inline constexpr float kSensorAttack = 0.005f;    // light up: fast
inline constexpr float kSensorDecay = 0.120f;     // light down: LDR memory
inline constexpr float kSensorFlickerHz = 100.0f; // mains lamp ripple (2 x 50 Hz)
inline constexpr float kSensorFlickerDepth = 0.15f;

// ---- Papa Srapa (rate range / divider ratios / x10 factor unspecified)
inline constexpr float kSrapaRateMin = 0.08f, kSrapaRateMax = 12.0f; // x1 position
inline constexpr float kSrapaRateX10 = 10.0f;
inline constexpr int   kSrapaDividers[] = { 1, 2, 4, 8, 16 };
inline constexpr float kSrapaPitchSpanOct = 5.33f; // PITCH knob span; manual: audio osc ~C0..E7
inline constexpr float kSrapaHiShiftOct = 2.0f;    // hi/low range switch offset (low C0.., hi C2..E7)
inline constexpr float kSrapaFmDepthOct = 3.0f;    // pitch jump at FM AMOUNT full (siren interval)
inline constexpr float kSrapaAmSlewSec = 30.0e-6f; // AM chop edge (~30 us keeps a soft click, no thump)
inline constexpr float kSrapaDutySpread = 0.10f;   // Schmitt duty = 0.5 +- spread * unit tolerance... never 50 %
inline constexpr float kSrapaDutyBias = 0.08f;     // ...plus a fixed charge/discharge asymmetry
inline constexpr float kSrapaLevelVolts = 4.0f;    // voice peak level into the mixer
inline constexpr float kSrapaCvOctPerVolt = 0.5f;  // panel cv-in pitch law (manual is silent; Schmitt
                                                   // osc CV pull, gentler than V/oct — by ear, M8 style)

// ---- VCO A/B (AS3340; base octave / FM gain / sub mix unspecified)
inline constexpr float kVcoLowBaseV = 1.0f;        // "low" octave: knob min + V/oct 0 = C1
inline constexpr float kVcoOctUpV = 3.0f;          // oct+3 switch
inline constexpr float kVcoLinFmHzPerVolt = 60.0f; // linear FM gain at CV AMT full (non-through-zero)
inline constexpr float kVcoLevelVolts = 5.0f;      // audio out +-5 V (spec p4) — level, not by-ear
inline constexpr float kVcoSubMix = 0.7f;          // -1 sub square level relative to the main wave
inline constexpr float kVcoPwMin = 0.05f, kVcoPwMax = 0.95f;
inline constexpr float kVcoToleranceCents = 3.0f;  // per-unit 3340 trim error (precision chip, tiny)

// ---- FV-1 effector interface (chip levels are documented; the analog
// scaling around the chip is not). ADC full scale mapped to +-8 V so the
// post-DIST signal drives the converters like the hardware line level does;
// unity ADC->DAC keeps a bypass program transparent. WET-out max ~2 V (spec
// p4) emerges from program levels — calibrate in M8 against SOUND DEMO 3.
inline constexpr float kFv1FullScaleVolts = 8.0f;
inline constexpr float kFv1ClockSkew = 0.0005f;   // channel R crystal +-0.05 %
inline constexpr float kFv1PotSmoothSec = 0.02f;  // RC on the pot/CV line
inline constexpr float kFv1CvPerPot = 1.0f / 20.0f; // POTn = knob + CV/20 (X/Y/Z CV -10..+10 V)
inline constexpr float kBlendSmoothSec = 0.01f;

// ---- Keyboard firmware (0-255 / 0-127 units -> physical values, unspecified)
inline constexpr float kPortamentoMaxSec = 2.5f;  // at 255 (square-law knob feel)
inline constexpr float kPressureSlewMaxSec = 4.0f;// rise/fall at 255
inline constexpr float kVibratoMinHz = 0.1f;      // speed 0
inline constexpr float kVibratoMaxHz = 12.0f;     // speed 127
inline constexpr float kVibratoMaxSemis = 1.0f;   // depth 127, peak pitch swing
inline constexpr float kVibratoDelayMaxSec = 2.0f;// delay 127, ramp-in time
inline constexpr float kKbGateDuty = 0.5f;        // arp/seq gate = half a clock step

} // namespace s42::tuning
