# M8 — Calibration-by-ear protocol

The M8 loop: **listen → verdict → move constants → re-render → repeat**, then
freeze. Every candidate value lives in `app/src/dsp/TuningConstants.h` and
nowhere else; a calibration iteration edits exactly that file.

## The kit

```
cd app
cmake --build build --target solar42n_calib
./build/solar42n_calib_artefacts/RelWithDebInfo/solar42n_calib ../renders/calib          # all scenes
./build/solar42n_calib_artefacts/RelWithDebInfo/solar42n_calib ../renders/calib filter   # one scene
```

Each scene is rendered through the **real processor** (the exact knob→physical
maps, smoothers and tolerances the app uses) and prints its timeline sheet.
Context checks on full patches: the factory-preset renders
(`renders/solar42n-m7-preset-*.wav`, `solar42n-m4-shimmerpath.wav`,
`solar42n-m6-keyboardarp.wav`) and, always better, the standalone app played
live.

## References

- **SOUND DEMO 3** (Elta official) — the primary A/B target per the plan.
- Hardware demo videos (YouTube: Elta Music Solar 42/42F/42N demos) — pick and
  name specific moments below as criteria firm up; a verdict that cites a
  video timestamp is worth double.
- The manual gives **no numbers** for any of this (07 §6) — the ear is the
  spec.

## Scenes → constants → verdicts

Verdict style: **PASS** (freeze it) / **ITERATE** (say which way: "max attack
too short", "dirty zone starts too late"). Log every verdict in `00-LOG.md`.

| # | Scene (WAV in `renders/calib/`) | Listen for | Constants on trial | Verdict |
|---|---|---|---|---|
| 1 | `calib-drone-env` | swell/fade extremes usable? knob-0 clicky enough, knob-1 "endless" enough? | `kDroneAttMin/Max`, `kDroneRlsMin/Max` | — |
| 2 | `calib-vco-env` | percussive at min; pad-slow at 0.8; LOOP as LFO musical? | `kVcoAtt/Dec/RelMin/Max`, `kEnvAttackOvershoot` (snap) | — |
| 3 | `calib-lfo-ranges` | A: 0.02–5 Hz × ranges; B: 0.2–50 Hz; x10 into audio-rate growl wanted? | `kLfoAMin/Max`, `kLfoBMin/Max` | — |
| 4 | `calib-pulser` | slow end slow enough, fast end fast enough for seq/arp use | `kPulserMin/Max` | — |
| 5 | `calib-volt-dirty` | transpose span; growl onset at ~knob 0.5; full-dirt intensity | `kVoltTransposeOct`, `kVoltDirtyStart`, `kVoltCrossFmMax` | — |
| 6 | `calib-filter` | useful cutoff ends; sing onset ~knob 0.85; bounded scream keeps bass | `kFilterFreqMin/Max`, `kFilterSelfOscRes`, `kFilterNegDamp`, `kFilterDrive*` | — |
| 7 | `calib-dist` | GAIN silent at DIST 0 (by design); drive intensity; equal loudness across DIST | `kDistDriveMax`, `kDistStage*V`, `kDistMakeup` | — |
| 8 | `calib-sensor` | vactrol asymmetry (fast up / ~120 ms down); room-light floor; flicker depth | `kSensorAttack/Decay`, `kSensorFlickerHz/Depth`, `kDroneCvOctPerVolt` | — |
| 9 | `calib-srapa` | chop-rate span + x10; pitch span C0–E7 feel; hi shift; siren FM depth | `kSrapaRate*`, `kSrapaPitchSpanOct`, `kSrapaHiShiftOct`, `kSrapaFmDepthOct` | — |
| 10 | `calib-panlaw` | flat loudness across the pan; audible L/R filter character difference | pan law, `kFilterLrSkew`, tolerance magnitudes | — |
| 11 | `calib-fx-wet` | per-cartridge voicing full-wet; wet ceiling ≈ −6 dBFS (2 V) | `kFv1FullScaleVolts`, program `.spn` levels | — |

## Whole-patch A/B criteria (plan §Verification, refined as verdicts land)

Against SOUND DEMO 3 / named videos, on the factory presets + live play:

1. **Beating density** — detuned 5-gen clusters shimmer at the demo's rate.
2. **Dirty-zone growl** — VOLT past noon misbehaves like the hardware.
3. **Filter scream** — self-osc is angry but bounded, bass survives.
4. **Shimmer darkness** — CATHEDRAL's ~15 kHz FV-1 brickwall haze.
5. **Stereo liveness** — L/R never phasey-identical; a "unit", not a preset.

## Freezing (end of M8)

When a row goes PASS: mark it here and stop touching those constants. When
all rows PASS: bless offline-render goldens (`solar42n_render` +
`solar42n_calib` outputs hashed in `check.sh`), final entry in `00-LOG.md`,
milestone table flip in `08-implementation-plan.md`.

## Session log

- 2026-07-05 — kit built (11 scenes), first full render in `renders/calib/`.
  No verdicts yet. Analytical pass over `TuningConstants.h` vs the 07 voltage
  table found no contradictions; all values remain as-authored pending ear.
