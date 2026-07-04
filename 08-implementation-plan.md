# Solar 42N — Digital Instrument: Full Implementation Plan

> **This is the approved plan of record** (2026-07-03), copied from the
> planning session into the repo. **Milestone status below is the durable
> cross-session tracker** — update it (plus `00-LOG.md`) at the end of every
> milestone/feature, per the ritual in `CLAUDE.md`.

## Milestone status

| Milestone | Status | Evidence |
|---|---|---|
| M0 — Skeleton + knowledge capture | ✅ done | commit `4eb9724`; check.sh green; pluginval SUCCESS; `07-42n-panel-inventory.md` |
| M1 — First sound: classic drone voice | ✅ done | commit `e2f1c05`; 13/13 tests; 4-point BLEP −61 dBc; `renders/solar42n-m1-drone1.wav` |
| M2 — Rack & patching core | ✅ done | commit `c80f612`; 21/21 tests incl. feedback + patch fuzz |
| M3 — Full voice complement + Polivoks filters | ✅ done | commit `0cdc662` (2026-07-04); 41/41 tests (5 srapa modes, filter self-osc/no-bass-loss, VCO sync/sub/alias, pan-routing); pluginval SUCCESS; panel UI phase 1 (all top-half + mod-strip sections live); `renders/solar42n-m3-fullpath.wav` — ear check superseded by the M4 render |
| M4 — FV-1 VM + starter cartridges | ✅ done | commit *(pending)* (2026-07-04); 55/55 tests (per-opcode fixed-point goldens, assembler word-identical to asfv1 on AN-0001 + all 12 programs, octave-up pitch-shift FFT, shimmer bloom, OCHRE one-shot trigger/re-arm, resampler >90 dB SNR, effector unity round-trip); pluginval SUCCESS; `renders/solar42n-m4-shimmerpath.wav` — **ear check pending** |
| M5 — Full panel UI + cable layer | ⬜ next | |
| M6 — Touch keyboard + drone keypad | ⬜ | |
| M7 — State, presets, conveniences | ⬜ | |
| M8 — Calibration by ear | ⬜ | first user verdict logged 2026-07-03 (00-LOG); criteria to be co-developed |
| M9 — RT-safety, performance, MIDI, release | ⬜ | |

## Context

The project so far produced research docs (`00`–`06`), official Elta manuals in `reference-docs/`, and a throwaway web prototype (`drone-lab/`) that models an *incorrect* simplification of the drone section. The goal now is the real thing: a **complete, polished digital recreation of the Elta Solar 42N** — panel interface as close as possible to the hardware, synthesis as close as possible to the hardware — built as a **native C++ JUCE app/plugin**.

**Decisions locked with the user (2026-07-03):**
1. **Platform**: Native C++ **JUCE 8** — Standalone + AU + VST3, macOS first, CMake build. (drone-lab stays untouched as a reference experiment.)
2. **Revision**: **Solar 42N** (matches our precise panel render `reference-docs/solar42n-panel-1.png`; feature superset).
3. **Patching**: **Full virtual patch cables** — every jack functional, hardware-style normals that break when patched, signals modeled in volts.
4. **FX scope**: cartridge **framework + starter set** (CATHEDRAL, TIME, VIBROTREM, OCHRE ≈ 12 programs); remaining 9 cartridges are follow-on backlog on the same framework.
5. **Effector**: **FV-1 chip-level emulation** — a fixed-point VM for the Spin FV-1 + our own SpinASM programs approximating Elta's cartridges (the actual cartridge ROMs are proprietary; we author approximations from the X/Y/Z catalog).

Stated defaults (not asked, follow from the fidelity goal): character-critical DSP is custom (PolyBLEP oscillators, nonlinear Polivoks filter model, FV-1 VM at 32,768 Hz); strictly faithful behavior first, digital conveniences (tooltips, presets, MIDI) layered on top without breaking faithfulness; photo-sensors emulated with a virtual "room light" model (webcam-luma = documented stretch, not scheduled).

## Reference facts (source of truth)

- `06-manual-digest.md` — verified architecture (uses base-42 voice numbering).
- `reference-docs/solar42n-panel-1.png` — the 42N panel; geometry source for all UI. Panel 49.5 × 32 cm.
- **42N naming/deltas vs the digest** (from this session's manual sweep — must be persisted in M0, see below): voices renumbered **DRONE 1,2,4,5 = classic 5-saw; DRONE 3,6 = Papa Srapa; VCO A/B**; **VCO A has SYNC in, VCO B has OSC out** (swapped vs base 42); filter **CV L normalled to CV R**, MOD L/MOD R = filter CV-amount knobs; BP/LP switch per filter + shared post-filter DIST + GAIN ("double distortion"); LFO A/B gain x1/x6/x10 range switches; VCO −1 sub-osc + lin/exp switches; 1‑2‑3 toggle loads effector program per channel; knob color code (blue=drone, green=VCO/env, orange=filter/FX, red=mod strip, grey=mixer VOL, black=PAN); jack labels red=out, black=in.
- Signal path: 8 voices + EXT AUDIO + PREAMP → 10-ch mixer (VOL + PAN; **pan position crossfades each channel between Filter L and Filter R** — pan IS the filter routing) → dual Polivoks-type 12 dB filter → DIST/GAIN → dual FV-1 (independent program per side; shared X/Y/Z + CV, BLEND, MASTER) → WET OUT L/R (~2 V max). VCO A/B pre-filter DRY OUTs (~1 V).
- Hardware normals (default patch): keyboard V/OCT → both VCO 1v/oct; keyboard GATE L → both envelope gates; VCO A out → VCO B cv (via CV AMT); each Papa Srapa's noise → own S&H in; pulser → sequencer clock; filter CV L → CV R; internal piezo → preamp; ambient light → photo-sensors.
- ~50 patch jacks total; voltage ranges per the digest's spec table (LFOs unipolar 0..+10 V — by design, to drive the photo-sensor LEDs; joystick ±10 V; seq CV 0–5 V; keyboard V/oct 0–8 V; drone env outs ±10 V; EG 0–8 V; S&H ±5 V; Papa Srapa mod out 0..+12 V).
- **Unspecified by manuals — estimate by ear** (all become named constants in `TuningConstants.h`): every envelope time/curve; LFO A/B Hz ranges; pulser range; filter cutoff range/self-osc point; Papa Srapa rate range + DIVIDER ratios; VOLT transpose depth + dirty-zone onset; photo-sensor transfer/lag; portamento/vibrato times; all FX algorithm internals (only X/Y/Z names known).

## Project layout & build

New subdirectory **`app/`** at project root; CMake ≥ 3.25; **JUCE 8 via FetchContent** pinned to an exact tag; **C++20**; `git init` at project root (`.gitignore`: `app/build*/`, `renders/`, the three large official PDFs — URLs preserved in `sources.md`).

Targets:
- `solar42n_dsp` — static lib, **zero JUCE dependency**: `PolyBlepOsc`, `SchmittOsc`, `TriCoreVco`, `RcEnvelope`, `PolivoksFilter`, `Waveshaper`, `SampleHold`, `NoiseGen`, `MorphLfo`, `EnvFollower`, `PolyphaseResampler`, `Tolerances`, `Volts`, `TuningConstants.h`.
- `solar42n_fv1` — static lib, JUCE-free: `Fv1Vm`, `Fv1Opcodes`, `Fv1Lfo`, `DelayRam`, `SpinAsm` (assembler), `CartridgeLibrary`.
- `solar42n_engine` — static lib, JUCE-free: `Rack`, `Jacks.h` (registry), `RoutingTable`, `CommandQueue`, `Telemetry`, `modules/` (ClassicDroneVoice, PapaSrapaVoice, VcoVoice, EnvelopeModule, MixerModule, FilterBank, EffectorModule, LfoModule, JoystickModule, StepSequencer, PreampModule, PhotoSensor, MasterOut), `keyboard/` (TouchKeyboard firmware logic, Quantiser, Arpeggiator, KbSequencer, PressureShaper, KbPresets).
- `Solar42N` — `juce_add_plugin(FORMATS Standalone AU VST3)`: `plugin/`, `ui/`, `state/`.
- `solar42n_tests` — Catch2 v3 (FetchContent).
- `fv1asm` — CLI wrapper of the assembler; `solar42n_render` — headless rig-state → WAV regression renderer.
- `scripts/check.sh` — build + tests + pluginval + render regression.
- `app/programs/<cartridge>/<n>-<name>.spn` — SpinASM sources, embedded via `juce_add_binary_data`.

## Engine architecture (the crux)

- **One `Rack`**, fixed hardware-mirroring process order: clock/keyboard → LFOs → joystick → sequencer → preamp/follower → drones 1–6 → VCOs + envelopes → mixer → filters → effector → master.
- **Everything audio-rate, everything in volts.** Each output jack (`Outlet`) owns a float buffer; inputs (`Inlet`) resolve `patched ? patchedOutlet : normalOutlet` — hardware normals break automatically with zero special-casing.
- **`Jacks.h` = single X-macro/constexpr registry** of every jack (id, direction, module, normal source, voltage range). Engine, UI, and serialization all consume this one table.
- **Feedback allowed** (hardware allows it): fixed **64-sample internal sub-blocks**; outlets double-buffered; a cable pointing "backwards" in process order reads the previous sub-block (~1.3 ms loop delay). Every inlet soft-clamps to ±12 V rails (feedback saturates musically, never NaN); denormal flush throughout.
- **Cable edits are RT-safe** via a lock-free SPSC `CommandQueue` (POD connect/disconnect commands, drained at sub-block boundaries into a plain `int16 sourceOf[numInlets]` routing table). Message thread keeps the canonical cable list in a ValueTree.
- CPU budget: ~26 saws + 4 Schmitt oscs + 2 VCOs + 10 envelopes + 2 nonlinear SVFs + 2 FV-1 VMs @32 kHz + resamplers ≈ well under 5% of one modern core — per-sample CV everywhere is affordable and buys clickless gates + audio-rate filter FM.

## DSP module designs (key choices)

- **Classic drone gen (5 × 4 voices)**: PolyBLEP saw; TUNE maps exponentially over each gen's factory range (C0–G2 / B1–G3 / D3–B4 / E4–C6 / G5–D7); per-gen fixed random cents offset from the tolerance seed; **no drift with MOD off** (stability is stock). **VOLT**: 0–50 % = transpose all gens down (~2–3 oct, tune by ear); past ~50 % ramp in **cross-FM** (each gen's phase inc modulated by the sum of the others) = the dirty zone. MOD button routes CV jack *or* photo-sensor to that gen's pitch. `PhotoSensor`: LED drive (unipolar-clamped CV) → LDR with vactrol-style asymmetric slew + room-light offset + optional 50/60 Hz flicker. AR + VCA + HOLD; env out ±10 V.
- **Papa Srapa (×2)**: `SchmittOsc` as RC relaxation oscillator (authentic non-50 % square, voltage-dependent rate; polyBLEP edges). Sub-audio osc (RATE, x1/x10, cv out 0..+12 V) + audio osc (PITCH, hi/low, ~C0–E7); FM toggle (depth = MOD, integer flip-flop DIVIDER), AM toggle (chop with ~30 µs slew), NOISE switch; S&H (in normalled to own noise, rising-edge clock jack, out ±5 V).
- **VCO A/B (AS3340 model)**: triangle core; wave rotary with two morph zones — saw↔inverted-saw (phase-warped variable-symmetry ramp + polyBLEP) and sine↔triangle (variable polynomial shaper) — plus square w/ PWM; sub = f/2 flip-flop; **hard sync** (VCO A) via phase reset with BLEP correction; linear FM non-through-zero (clamps ≥ ~0 Hz like a real 3340); lin/exp CV switch; oct+3/low; TUNE = 1 oct. Envelope A/B: `RcEnvelope` ADSR (exponential RC segments, attack charges toward ~1.3× overshoot target for analog snap) + HOLD (VCA open) + LOOP (self-retrigger = env-as-LFO); vca cv in.
- **Polivoks filter (L/R)**: **nonlinear 2-pole ZDF/TPT state-variable** with (a) asymmetric input drive clip, (b) hard-ish saturator in the resonance feedback path (screams into bounded self-oscillation ≈ RES > 0.85), (c) no-bass-loss modification (resonance does not thin bass — documented trait). BP/LP per side; LINK; CV L→CV R normal; MOD L/R CV amounts; post DIST + GAIN = two cascaded asymmetric waveshapers (GAIN drives, DIST crossfades clean↔dirty). **L/R ±2–3 % component-tolerance skew**.
- **LFO A/B**: square core through a variable slew limiter (WAVE knob = slew time: square→trapezoid→triangle — true analog morph); **unipolar 0..+10 V**; ranges ~0.02–5 Hz (A) / ~0.2–50 Hz (B) × x1/x6/x10 (constants, tune by ear).
- **Joystick**: position-locking; X/Y ±10 V with offset-knob window behavior (left −10..0 / mid −5..+5 / right 0..+10). **Sequencer**: 5 × CV 0–5 V + gate toggles, STAGES 3/4/5, PULSER internal clock (±10 V out) with ext-clock auto-switch. **Preamp**: up to 40 dB, 1 MΩ, soft rail clip; input = sidechain bus (standalone: input device). **Follower**: rectify → peak follower (attack/release) → env 0–10 V + Schmitt gate 0–8 V.
- **Mixer/output**: per-channel VOL (audio taper) + PAN as constant-power crossfade between Filter L and Filter R buses; BLEND dry tapped post-filter; WET calibrated 2 V peak ≈ −6 dBFS; DRY OUTs = optional second stereo output bus.
- **"Live stereo" philosophy**: `Tolerances.h` — deterministic PRNG seeded by a persisted **unit serial** (re-seedable = "swap unit"); every paired/multi element pulls fixed offsets from it (filter params, FV-1 clocks ±0.05 %, gen cents, VCA gains, envelope times).
- Every by-ear estimate lives in **`dsp/TuningConstants.h`** with a citation comment — the calibration milestone edits exactly one file.

## FV-1 effector

- **Fixed-point core** (the character lives in the math): ACC/regs/RAM 24-bit S.23 in `int32_t`, 64-bit MAC intermediates, exact saturation; coefficients decoded at real widths (S1.14 / S1.9 / S.10) from genuine 32-bit opcode words → the VM also runs **any community-assembled FV-1 binary** (huge for validation). Float build flag kept as a debug reference.
- Full documented ISA: RDA/RMPA/WRA/WRAP/RDAX/RDFX/WRAX/WRHX/WRLX/MAXX/MULX/LOG/EXP/SOF/AND/OR/XOR/SKP(flags)/WLDS/WLDR/JAM/CHO RDA·SOF·RDAL/CLR/NOT/ABSA/LDAX; SIN0/1 + RMP0/1 LFOs with full CHO semantics (**the ramp + NA-crossfade pitch-shifter idiom is the acceptance test** — CATHEDRAL shimmer and OCHRE reverse depend on it); 32768-word delay RAM with auto-decrementing pointer.
- **POTs**: POTn = clamp01(knob + CV/20), one-pole ~20 ms smoothing, optional 9-bit quantize (character toggle, default on; the FV-1 architecture doc specifies the pot ADCs are deliberately 9-bit with hysteresis — corrected from the plan's original 10-bit guess during M4). X/Y/Z + BLEND shared across both channels; program per channel via the 1‑2‑3 toggles.
- **Rate conversion**: host ↔ 32,768 Hz via custom Kaiser-windowed polyphase sinc resampler (~32 taps, JUCE-free, unit-tested) — the ~15 kHz brickwall is part of the tone. **Channel R clock skewed ±~0.05 %** from the tolerance seed (two slightly-different chips = live stereo).
- **Programs**: runtime **SpinASM assembler** (~600 LOC: labels, EQU, MEM, literals, SKP offsets) compiling embedded `.spn` sources; `fv1asm` CLI for cross-checking binaries against reference assemblers (asfv1). Starter cartridges authored in-house from the digest's X/Y/Z catalog: **CATHEDRAL** (shimmer / oct-up delay / space reverb), **TIME** (delay-reverb / delay-chorus / delay-vibrato), **VIBROTREM** (tremolo / vibrato / chorus), **OCHRE** (reverse one-shot long/short / free-run loop).
- **Cartridge slot semantics (hardware-faithful)**: flipping a channel's 1‑2‑3 switch loads that program from the currently-inserted cartridge into that channel's VM (RAM cleared, LFOs JAMmed); removing the cartridge changes nothing until a switch flips; L and R can hold programs from different cartridges; persists in state.

## GUI

- `PanelEditor` in a logical **4950 × 3200** coordinate space; one JUCE Component per silk-screened section (4× ClassicDroneSection, 2× PapaSrapaSection, 2× VcoSection, 2× EnvelopeSection, EffectorSection, FilterSection, MixerSection, 2× LfoSection, JoystickSection, StepSequencerSection, PreampSection, EnvFollowerSection, MasterIoSection, TouchKeyboardSection, DroneKeypadSection).
- **Code-drawn vector** (juce::Path + `SolarLookAndFeel`), not bitmaps: static print layer (panel cream, section boxes, labels, red routing arrows, wordmarks) cached per zoom level; live widgets on top. All positions in **`PanelLayout.h`**, a constexpr table measured off the reference PNG. Widget kit matches the hardware color code (knobs blue/green/orange/red/grey/black, hex-nut jacks, slide switches, LED bars, white sensor windows). **Trade dress**: for anything distributed, replace Elta wordmarks/logos with original branding ("inspired by").
- **Fixed 4950:3200 aspect, resizable + zoom 100–300 %** (Cmd+scroll / buttons), space-drag pan, double-tap section header to zoom-to-section.
- **`CableLayer`**: press jack → ghost cable, compatible jacks highlight; release to connect; grab plug end to re-route; Alt-click unplugs; one cable per inlet, many per outlet (built-in mult); bezier with gravity sag, plug bodies, ~85 % opacity; normals shown as faint dashed traces on hover/toggle; hit targets ≥ 24 px.
- Knob conventions: vertical drag, Shift = fine, double-click = default, scroll steps, value bubble in volts/Hz/s, Cmd-click = type value.
- **Touch keyboard**: 12 `TouchPlate`s — **mouse Y within the plate = pressure** (capacitive-area faithful), horizontal drag glides; transpose buttons; **faithful encoder + display emulation** (firmware menu pages as a state machine in `engine/keyboard`) **plus** a parallel modern `SettingsDrawer` exposing the same state (behaviour single/twin/split, mode keyboard/arp/seq per side, quantiser with 19 scales + per-note editor + root, portamento/legato, vibrato, pressure modes raw/ASR/AD/loopAD/random + rise/fall, arp hold/mult-div/4 directions/variation ×1–3 + interval/rhythm mask, seq 2–16 steps + editors, clock 10–300 BPM auto-external, presets A–D). Drone keypad: 6 buttons (columns 1,4 / 2,5 / 3,6) gating drone voices.
- **Photo-sensor UI**: sensor windows glow with LED drive from telemetry + room light; global **Room Light** slider + flicker toggle; mouse-hover acts as a flashlight over a sensor.
- **Telemetry**: engine fills a preallocated seqlock-guarded struct (~128 floats: gen LEDs, env levels, seq step, meters) per sub-block; UI samples at 30–60 Hz. No locks/allocation on the audio thread.

## State, presets, MIDI

- **APVTS** for ~200 automatable params, IDs `section.control` (`d1.gen3.tune`, `filtL.freq`, `fx.x`…), ~10 ms smoothing at the engine edge.
- ValueTree children: `CABLES` (jack-id string pairs), `KEYBOARD` (full config incl. microtuning + presets A–D), `CARTRIDGES`, `TOLERANCES` (unit serial), `UI`.
- Presets: full-rig `.s42n` snapshots; factory presets (manual patch examples + demo-style drones) embedded; user presets in `~/Library/Application Support/Solar42N/Presets`; preset bar lives outside the skeuomorphic panel.
- MIDI (M9): notes → touch keyboard (velocity/poly-AT → pressure), MIDI clock → clock ins, right-click CC learn. Strictly an adapter layer.

## Milestones

- **M0 — Skeleton + knowledge capture.** `git init`; `app/` CMake + JUCE 8; all targets build; empty plugin passes pluginval strictness 5; Catch2 runs; `scripts/check.sh` green. **Write `07-42n-panel-inventory.md`** persisting this session's manual sweep: full control/jack census per section, normalling list, 42N deltas (incl. VCO A sync-in / VCO B osc-out swap), voltage table, and the unspecified-values list.
- **M1 — First sound: one classic drone voice.** PolyBlepOsc + RcEnvelope + ClassicDroneVoice (5 gens, ranges, MUTE, VOLT + cross-FM dirty zone, AR/VCA/HOLD) → placeholder filter → out; temporary slider UI. *Verify: tune a beating 5-gen cluster by ear; dirty zone growls; alias-floor unit test.*
- **M2 — Rack & patching core.** Jack registry, volt buffers, normals, RoutingTable + CommandQueue, 64-sample sub-blocks + double-buffered outlets, rail clamps; LFO A/B, joystick, sequencer, pulser, preamp + follower; debug patch-matrix UI. *Verify: LFO→drone CV patch works; feedback patch stays bounded; routing unit tests; connect/disconnect fuzz.*
- **M3 — Full voice complement + real filters.** Papa Srapa (5 documented modes + S&H), VCO A/B (+ morphs, sync, sub, lin/exp) + ADSR/LOOP/HOLD, all normals live, 10-ch mixer with pan-as-filter-routing, Polivoks ZDF-SVF + tolerances + DIST/GAIN. Panel UI phase 1 in parallel (theme, widget kit, top-half layout). *Verify: full analog path audible; filter response + self-osc tests; the 5 Papa Srapa modes vs manual descriptions.*
- **M4 — FV-1.** VM (fixed-point, full ISA, LFO/CHO), SpinAsm + fv1asm, per-opcode golden tests + run a known community binary, PolyphaseResampler, dual skewed VMs, POT/CV mapping, cartridge semantics, author the 12 starter programs. *Verify: shimmer on a drone through the full chain; assembler output word-identical to asfv1; OCHRE reverse one-shot triggers.*
- **M5 — Full panel UI + cables.** Complete `PanelLayout` (every control + ~50 jacks), cached print layer, CableLayer interactions, LEDs/telemetry, zoom/pan; delete debug UIs. *Verify: side-by-side screenshot vs `solar42n-panel-1.png`; every jack patchable.*
- **M6 — Touch keyboard + drone keypad.** Firmware logic as pure unit-tested code (behaviours, modes, quantiser + 19 scales + per-note editor, portamento, vibrato, pressure modes, arp, 16-step seq, clock, presets A–D); plate UI + encoder/display + settings drawer; keyboard jacks/normals live. *Verify: glide on plates; arp locked to patched pulser clock; quantiser-off microtonal plate tuning.*
- **M7 — State & conveniences.** Full save/recall round-trip (params + cables + keyboard + cartridges), factory presets, tooltips, room-light control, automation naming pass. *Verify: DAW kill/relaunch → identical sound; click-free preset switch.*
- **M8 — Calibration ("tuning by ear").** Systematic pass over `TuningConstants.h` vs SOUND DEMO 3 + hardware videos: envelopes, LFO/pulser ranges, dirty-zone onset, filter drive/self-osc, sensor lag, FX voicing, pan law, tolerance magnitudes. Freeze constants; commit offline-render goldens; log verdicts in `00-LOG.md` style.
- **M9 — RT-safety, performance, MIDI, release pass.** Audio-thread audit (no alloc/locks; malloc guard + sanitizer), CPU measurement (< ~10 % of a core @ 48 k/128), pluginval strictness 10 + auval, `solar42n_render` regression in `check.sh`, MIDI note/clock/CC-learn, signing/notarization notes.
- **Backlog (post-plan)**: remaining 9 cartridges (MAGIC, FILTER, VIBE, PITCH SHIFTER, INFINITY, STRING RINGER, SYNTEX-1, DIGITAL, GENERATOR); webcam room-light; Windows/Linux builds; original branding pass before any public release.

## Verification

- **Unit tests**: polyBLEP alias floor (FFT, reflected partials < −70 dB); RC envelope timing; SVF magnitude vs analytic + bounded self-osc; resampler SNR > 90 dB; S&H edges; quantiser tables; arp/seq logic; **FV-1 per-opcode goldens** + whole-program fixtures + assembler binary equivalence vs asfv1.
- **Routing tests**: normals break/restore; feedback finite; command-queue fuzz under render — no NaN/crash.
- **Regression**: `solar42n_render` renders committed rig states each `check.sh` run; deterministic via fixed tolerance seed → hash/spectral compare; goldens re-blessed explicitly.
- **pluginval** strictness 10 (AU + VST3) + auval in `check.sh`; RT-safety malloc-guard in debug callback.
- **Final gate = ear**: A/B checklist vs SOUND DEMO 3 + named hardware demos (beating density, dirty-zone growl, filter scream, shimmer darkness, stereo liveness), PASS/ITERATE logged per the house `00-LOG.md` methodology.

## Risks

1. **FV-1 ISA edge cases** (SKP/PACC, CHO RMP + NA crossfade, LOG/EXP corners) → opcode goldens from the datasheet; validate against community binaries; float diagnostic build to bisect quantization vs logic.
2. **Patch feedback instability** → rail clamps at every inlet + 1-sub-block loop delay + hostile-patch fuzz; worst case loud-but-musical, like hardware.
3. **GUI polish tar pit** → single `PanelLayout` table + small widget kit; screenshot-diff vs reference PNG defines "done"; timeboxed beauty passes (M5, post-M8).
4. **Touch keyboard scope** (product-within-product) → pure logic, unit-tested, UI-independent; settings drawer is the functional path, faithful display emulation can trail; presets A–D and per-note editor are designated cut lines.
5. **Trade dress** — layout/behavior are fair game; replace Elta wordmarks before anything distributed.
6. **CPU** — low risk (< 5 % budgeted); measured from M4 in `check.sh`.

## Critical files

- `06-manual-digest.md`, `reference-docs/solar42n-panel-1.png` (+ `solar42n-manual.txt` for 42N deltas) — sources of truth
- New: `07-42n-panel-inventory.md` (M0), `app/src/engine/Jacks.h`, `app/src/engine/Rack.h`, `app/src/fv1/Fv1Vm.h`, `app/src/fv1/SpinAsm.h`, `app/src/dsp/TuningConstants.h`, `app/src/ui/PanelLayout.h`, `app/programs/**/*.spn`, `scripts/check.sh`
