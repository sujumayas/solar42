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
| M4 — FV-1 VM + starter cartridges | ✅ done | commit `ccb56b6` (2026-07-04); 55/55 tests (per-opcode fixed-point goldens, assembler word-identical to asfv1 on AN-0001 + all 12 programs, octave-up pitch-shift FFT, shimmer bloom, OCHRE one-shot trigger/re-arm, resampler >90 dB SNR, effector unity round-trip); pluginval SUCCESS; `renders/solar42n-m4-shimmerpath.wav` — **ear check pending** |
| M5 — Full panel UI + cable layer | ✅ done | commit `cd9f2f6` (2026-07-04); 61/61 tests (telemetry seqlock torture, jack-census static_assert + geometry/spacing, name-lookup round-trip, preamp ext-jack override, dry-out scaling); pluginval SUCCESS; full 4950×3200 panel: 63-jack census in `PanelLayout.h`, CableLayer (ghost cable, re-route, Alt-unplug, normal traces on hover, one-per-inlet/mult-per-outlet), cables persist via CABLES state child, telemetry LEDs + sensor glow + step/clip/gate LEDs @30 Hz, zoom 100–300 % (Cmd+scroll/pinch) + pan + double-click zoom-to-section, DRONE VOICES keypad + joystick pad live, debug patch matrix deleted; `renders/m5-panel-screenshot.png` vs render — **panel eye check pending** |
| M6 — Touch keyboard + drone keypad | ✅ done | commit `13c2e08` (2026-07-04); 85/85 tests (+24 keyboard cases / ~1.9k assertions: quantiser 19-scale + microtonal + LOAD SCALE plate retune, last-note priority, portamento/legato glide, twin PRESSURE-as-right-Voct, split dual-mode, arp order/dir/variation/rhythm/HOLD/RESET, seq dir + gated-vs-continuous CV + key-run + plate transpose, clock auto-external + BPM-revert + mult/div scaler, 5 pressure modes, presets A–D, encoder menu + sub-editors, rack pulser→arp-clock normal); pluginval SUCCESS; firmware in `engine/keyboard/` (JUCE-free); playable `KeyboardSection` (plates=pressure, glissando, encoder+LCD menu) + parallel `SettingsDrawer`; KEYBOARD state child persists config+microtuning+presets; `renders/solar42n-m6-keyboardarp.wav` — **ear + panel eye check pending** |
| M7 — State, presets, conveniences | ✅ done | commit `5fe42d9` (2026-07-04); 87/87 tests (86 Catch2 incl. new effector loaded-slot semantics + `solar42n_statecheck` harness: randomized full-state round-trip → relaunched twins render sample-identical, all 6 factory presets load/render, click-free switch fades to true silence); pluginval SUCCESS; CARTRIDGES child persists what each FV-1 chip HOLDS (mid-swap slots survive), TOLERANCES child persists the unit serial (+ Swap Unit action), UI child persists editor size; PresetManager + preset bar (6 factory presets, user `.s42n` in ~/Library/Application Support/Solar42N/Presets, prev/next, faded loads keep *your* serial); tooltips on all 63 jacks (from the registry: direction/range/normal) + curated control hints; automation pass: parameter groups per section, real units (s/Hz/dB/V/%/L-R), knob value bubbles + double-click default; 1-2-3 combos → real 3-pos switches + cartridge-bay art; `renders/solar42n-m7-preset-srapa-aviary.wav`, `renders/solar42n-m7-preset-reverse-air.wav` — **ear check pending** |
| M8 — Calibration by ear | 🔶 in progress | kit commit `73e959e` (2026-07-05): `solar42n_calib` renders 11 per-domain audition scenes through the real processor into `renders/calib/`; listening protocol + verdict table in `09-calibration-protocol.md`; 88/88 tests; pluginval SUCCESS. Same session: preset location fix + legacy migration (user preset preserved), cross-build state merge (+statecheck case), cable-wipe-on-re-prepare engine bug fixed (+TestRouting case), standalone Bluetooth TCC crash fixed. **Ear loop pending: per-scene verdicts vs SOUND DEMO 3** |
| M9a — MIDI-in: keyboard + drone gates | ✅ done | commit `2d926f9` (2026-07-05): `MidiPerformer` (JUCE-free) + `KbTouch.plateShift`; C1–F1→drones (momentary or HOLD-latch via `midi.droneLatch` checkbox in the drawer), 36+→plates w/ per-note octave shift (60–71 native), velocity/AT→pressure, CC64/123; 94/94 tests (+6), pluginval SUCCESS; **hands-on KeyLab 37 verdict pending** |
| M9b — UI legibility & fidelity pass | 🔶 P1+P1.5+P2 done | **P1 done `41268ff` (2026-07-05)**: legibility floor, ChoiceSlideSwitch, DIVIDER knob, cartridge bay, envelope rebuild, filter-strip inset. **Eye check 2026-07-07 → ITERATE** (tile review vs `solar42n-panel-1.png`; 07 row-order doc bug fixed). **P1.5 done 2026-07-07, commits `58942b4`…`60ebf31`** (9 commits, every fix tile-diffed vs the reference before commit): drone rows to MUTE/TUNE/MOD + numbers on top, VCO pwm/pw captions freed, mixer print (PAN◆/L◄►R/black band/EXT.AUDIO/VOL, VOICE MIXER → center badge), effector to print layout (MOD L/R above CV jacks, BP/LP bracket, headphones silkscreen, cartridge slot), sensor white-at-idle (LED-only tint via `PhotoSensor::ledGlow`), envelope jack labels below nuts (row → fy 0.72), seq voltage ruler + stacked cv/gate (census `labelSide` field), joystick stack + LED, preamp gain size, SOLAR 42ᴺ lockup + diacritics + СОЛАР 42N + WET OUT bracket + dashed digital corner, KEYBOARD CV strip box, print-style title bands (TopRight/TopLeft/TopCentre; classic drones bandless) + S&H corner badge, VCO low label, d-pad dots, LED bars. check.sh ALL GREEN (94/94, pluginval SUCCESS); `renders/m9b-p15-panel-screenshot.png` — **user eye check pending**. **P2 done 2026-07-08, commits `23dacb5` (fix 16) + `e5fa794` (fix 17)**, both tile-diffed vs the reference: keypad → square keycaps (bevel, LED notch, numbers above), keyboard → 6\|6 ridged plate clusters around the centre group (white icon row, ▽/△ flanking red-centre encoder, blue LCD w/ OCT in status row, sunburst, △ feet, SETTINGS top-right chip). check.sh ALL GREEN; `renders/m9b-p2-panel-screenshot.png` — **user eye check pending**. **P3 done 2026-07-08, commits `4e25d41` (fix 18) + `27b949d` (fix 19) + `24c44d4` (fix 20)**, each tile-diffed vs the reference: jack I/O markers panel-wide (black ▸ in / red ▲ out beside every label), VCO morph glyph ring + numbered screws 1–7/8–14 + pwm routing arrow + −1 sub ⊓ (wave knob dropped to the print's position so the ring clears the switch row), LFO ∧/⊓ wave icons, srapa fm/am arrows + LFO dotted run + noise spark, joystick knob→jack arrows + 2nd LED, BP/LP response-curve icons, preamp mic glyph + two-line ext. source, envelope-follower bell-over-burst glyph, POWER polarity glyph + bracket. Known divergence: the print's S&H random-dot cluster has no room (our NOISE knob owns that corner). check.sh ALL GREEN (94/94, pluginval SUCCESS); `renders/m9b-p3-panel-screenshot.png` — **user eye check pending**. Fidelity backlog remains below |
| M9c — RT-safety, performance, release | ⬜ | original M9 scope minus M9a (MIDI clock + CC-learn live here) |

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
- MIDI (M9a, scoped 2026-07-05 — see the **M9 breakdown** for the binding spec): notes → touch plates with per-note octave shift, C1–F1 → drone gates (momentary-or-latch checkbox), velocity/aftertouch → pressure, CC64 sustain. Strictly an adapter layer. MIDI clock → clock ins and right-click CC learn deferred to M9c.

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

### M9 breakdown (scoped 2026-07-05 in session with the user)

Context: the user has **no hardware Solar 42 on hand** to A/B against, so the M8
ear loop is paused and M9 was re-sliced to pull the **digital-only features**
forward. These specs are decisions made *with the user* — treat them as locked.

**M9a — MIDI-in: play the Microtonal Drone Synthesizer + drone voices from a
MIDI keyboard (target controller: Arturia KeyLab 37).**

- **Seam**: parse the `MidiBuffer` in `processBlock` — the JUCE standalone
  wrapper delivers device MIDI through the same buffer, so ONE handler serves
  Standalone + AU + VST3. Mapping logic lives in a JUCE-free unit-tested class
  (engine side), the processor only feeds it raw note/CC events. MIDI touches
  enter the exact same path as mouse touches (`KbTouch`), so quantiser,
  microtuning, twin/split, arp, seq and pressure modes all work from MIDI for
  free.
- **Note map (omni — all channels):**
  - **MIDI 24–29 (C1–F1) → drone voices 1–6** (d1…d6; d3/d6 are the Papa
    Srapas). Chromatic order = voice number order.
  - **MIDI 36+ → the 12 plates**: plate index = note mod 12 (C → plate 1).
    **Notes 60–71 (C4–B4) play plates at native tuning; each keyboard octave
    away shifts that touch ±1 V** — *without* moving the panel OCT buttons.
    This is the deliberate digital-only capability the user asked for:
    reaching octaves the hardware can't while the panel state stays put.
    Implementation: `KbTouch` grows a per-plate octave-shift field (JUCE-free
    engine change, unit-tested); same-plate collisions (one plate held at two
    octaves) resolve last-pressed-wins, release restores the survivor.
- **Dynamics**: note-on **velocity → touch pressure**; **channel + poly
  aftertouch** re-shape pressure while held (KeyLab 37 sends channel AT);
  the existing pressure modes (raw/ASR/AD/loopAD/random) apply downstream
  unchanged.
- **Drone gate mode — user decision: BOTH, via a persisted checkbox**
  (APVTS param, surfaced in the settings drawer): *momentary* = gate high
  while the key is held, exactly like the on-panel DRONE VOICES keypad;
  *toggle* = note-on flips that voice's HOLD latch (press once → drone stays,
  press again → off). The user explicitly wants to switch between both
  workflows.
- **CC64 sustain pedal**: holds plate touches; in momentary mode also holds
  drone gates.
- **Out of scope for M9a** (lives in M9c): MIDI clock → clock ins, CC learn.
- *Verify*: unit tests on the mapper (note→plate/shift/drone routing,
  velocity/AT pressure, sustain, octave-collision policy, both gate modes);
  hands-on test with the KeyLab 37; check.sh green.

**M9b — UI legibility & fidelity pass (full-panel review vs
`solar42n-panel-1.png`, 2026-07-05).**

Root cause of the user's complaints: everything scales linearly from the
4950×3200 space (~30 % on a normal window) and several labels were authored
too small even at 100 %. Approach: a **global legibility floor** (minimum
rendered font size for interactive text, scale-aware) + targeted control
enlargement — not per-label tweaking.

- **P1 — legibility & overlaps** (the user's direct complaints):
  1. Cartridge selector 2–3× bigger (combo + "CARTRIDGE"/"flip 1-2-3" captions).
  2. Dropdowns that are physical controls on hardware: Papa Srapa **DIVIDER
     combo → blue knob**, LFO A/B **x1/x6/x10 combo → 3-pos slide switch**,
     seq **STAGES combo → 3-pos slide switch** (same treatment as the M7
     1-2-3 switches).
  3. Envelope A/B: "hold"/"loop" labels clip outside the box; ADSR row
     cramped; bottom jack labels collide.
  4. Drone sections: bottom-edge jack labels (GATE/env/env out/clock/out)
     clipped by the section border; DRONE 3/6 S&H box collides with jack
     labels + voice-number badge.
  5. Filter strip: FREQ/RES/DIST/GAIN should be **large** orange knobs per
     the reference; rotated FILTER L/R labels clipped; BP↔LP switch lost its
     icons.
  6. Effector: CV X/Y/Z jacks crowd the X/Y/Z knobs.
  7. Minor: joystick "offset X/X/Y/offset Y" label collisions; pulser→clock
     red arrow crosses the pulser knob; seq gate toggles/LEDs tiny; mixer
     channel labels tiny + PAN row missing L•R marks.
- **P1 status: done** (commit `41268ff`). Eye check 2026-07-07 (tile-by-tile
  sips crops of `renders/m9b-p1-panel-screenshot.png` vs the reference, all
  claims re-checked against manual text + `07`): the P1 legibility fixes are
  verified in; the side-by-side review found the fidelity gaps below.

- **P1.5 — fidelity corrections (eye check 2026-07-07). STATUS: done
  2026-07-07, commits `58942b4`…`60ebf31`, check.sh green.** Implementation
  notes vs the plan below: fixes 8+9 shared one commit (both trivial paint
  groups); fix 6 was solved by raising the envelope jack row to fy 0.72
  (labels now below like the print — the labelAbove compromise is gone);
  fix 7/8 needed a `labelSide` (below/above/left) field in the jack census
  because two stacked, below-labeled jacks cannot fit a 302-unit strip —
  the print puts those labels beside the nuts; BP/LP prints sit at the
  bracket ends (top edge clips text above the bracket). Each fix lists
  *where / what / how / verify*. Ground rules so nothing breaks: work the
  numbered groups **in order, one commit per group**; parameter attachments
  and jack ids are never touched (layout/paint only — anything needing a
  param or census change is explicitly flagged); after every group rebuild,
  screenshot, crop the affected tile and diff it against the same reference
  crop before moving on.
  1. **Classic drone rows are vertically flipped** — `ClassicDroneSection`
     (`PanelSections.h`). Hardware prints **MUTE (top) / TUNE / MOD (bottom)**
     with column numbers 1–5 above the MUTE row (render spec + manual text
     "MUTE TUNE MOD"; `07` §1a had it backwards — corrected this session).
     Reorder the three rows + rotated edge labels + move numbers to the top;
     MOD buttons end up directly above the GATE/HOLD block. Layout-only:
     every widget keeps its param id. Verify: tile diff + click MOD/MUTE and
     confirm the right params move (value bubbles), save/reload state.
  2. **VCO pwm/pw knob captions invisible** — `VcoSection::resized()`. The
     `LabeledKnob` caption strip (bottom ⅕ of bounds) sits under the census
     jack-row hex nuts, so "pwm"/"pw" never show. Raise/shrink the two knob
     bounds (y≈0.57→0.50) so caption clears the jacks, matching the print
     (captions sit clearly above the jack row). Verify: captions legible at
     default window, no collision with 1v/oct-row jack labels.
  3. **Voice mixer print details** — `MixerSection`. Add "PAN ◆" caption
     above each pan knob + "L◄ ►R" corner marks per cell; channel names go in
     a **black band between PAN and VOL rows** (white text, "EXT.AUDIO" not
     "EXT"); "VOL" caption above each grey knob; thin cell separators; move
     "VOICE MIXER" from our full-width band to the small **center badge**
     between the envelopes (where the print has it). Redraw only. Verify:
     tile diff.
  4. **Effector/filter block** — `EffectorSection` + `FilterSection`.
     (a) MOD L / MOD R knobs move **up out of the filter strip** into the
     effector body, above the CV L / CV R jacks (hardware position); strip
     order becomes FREQ RES · CV L DIST link GAIN CV R · FREQ RES.
     (b) BP/LP mode: current print reads as three stacked labels ("BP/BP/LP")
     — replace with a compact 2-pos toggle, "BP" top / "LP" bottom, dotted
     bracket arcing FREQ↔RES like the print (curve icons in P3).
     (c) **Headphones out is missing entirely** (inventory §1g: jack +
     level): draw glyph + jack + small orange level knob right of MASTER
     VOLUME. Not a VoltBus/census jack (audio out, not patchable) → visual
     now, behavior decision in backlog.
     (d) BLEND + MASTER VOLUME captions above their knobs (print), keep the
     P1-enlarged cartridge picker (deliberate divergence: we need a named
     picker where hardware has a physical slot) but retitle it lowercase
     "cartridge slot".
     Risk: CV L/CV R/link jack positions shift → cables re-anchor by jack id
     (safe), but re-check CableLayer hover/click targets. Verify: tile diff +
     patch a cable onto CV L before/after the change.
  5. **Photo-sensor idle tint** — classic drone sections render the sensor
     window pink at idle; hardware is plain **white** (glow should appear
     only with LED/room-light activity). Fix the glow compositing baseline.
     Verify: white at Init, glows when a gate fires / room light raised.
  6. **Envelope jack captions** — `EnvelopeSection`. P1 flipped them above
     the nuts (49-unit gap below). Try the hardware-faithful position
     (below) by shrinking nut size while respecting the 26-unit font floor;
     if it can't fit, the label-above compromise stays and is documented
     here as final. Verify: no clipping at default window size.
  7. **Sequencer strip** — `SeqSection`. Add the signature striped
     voltage-ruler print filling the title band right of "5 STEP SEQ.
     VOLTAGE"; stack the cv (top) / gate (bottom) out jacks at the right
     edge per print. clock-out keeps its census jack + red label even though
     the render spec art hides it. Verify: tile diff.
  8. **Joystick block** — `JoystickSection`. Stack X/Y out jacks
     **vertically** between the offset knobs + small red LED (print);
     curved red arrows land in P3. Verify: tile diff.
  9. **Preamp** — `PreampSection`. gain becomes a medium red skirted knob
     per print (currently small); mic glyph in P3. Verify: tile diff.
  10. **Header & branding print** — `PanelView` paint.
      "SOLAR 42ᴺ": **42 in red + superscript N in black**, tight kerning
      (ours has black 42 + full-size red N); "ȦMBIĒNT MACHINE" diacritics
      (dot over A, macrons — draw as strokes if the font lacks them) + wide
      letterspacing; add the missing **"СОЛАР 42N"** print (black + red)
      above the keyboard block, right side; WET OUT bracket line + two-line
      "EXT. AUDIO" label; POWER polarity glyph in P3. ROOM LIGHT + 50 Hz
      (digital-only additions) get a thin outline box so they read as
      deliberate, clear of the POWER corner. Verify: header tile diff.
  11. **Keyboard CV jack row** (V/OCT…RESET) — not on the reference art
      (hardware wears these on the keyboard block itself). Box them in a
      thin outlined sub-panel snugged to the synth block's top-right corner
      so they read as part of the keyboard. Verify: bottom-right tile diff.
  12. **Section title bands → print-accurate** — `Section` base gains a
      band-style option. Hardware prints **no title** on DRONE 1/2/4/5
      (number badges + mixer labels identify them — remove our bands);
      DRONE 3/6 + VCO A/B use the **short top-right band** of the print
      (ours are full-width top-left); restore the small **"S&H" corner
      badge** on the Papa Srapa sample-hold box (lost in P1). Visual only,
      but drone sections gain ~60 units of height — re-run the drone-row
      layout after (do this group *after* fix 1). Verify: tile diffs.
  13. **VCO switch sub-labels** — "low" under the oct+3 switch (down
      position meaning), ⊓ glyph next to −1 sub in P3. Verify: tile diff.
  14. **JoyPad d-pad print** — solid black triangles + the 4 diagonal corner
      dots per print. Verify: bottom-left tile diff.
  15. **Drone LED bar styling** — red vertical-bar segments in a light bezel
      per print (currently round dots in a dark box); telemetry binding
      unchanged. Verify: tile diff + LEDs animate with gates.
- **P2 — hardware fidelity (updated with 2026-07-07 findings). STATUS: done
  2026-07-08, commits `23dacb5` (fix 16) + `e5fa794` (fix 17), check.sh
  green, both fixes tile-diffed vs the reference before commit.**
  16. DRONE VOICES keypad → six **square** pads, numbers printed **above**,
      small top-center LED notch per pad, tighter pitch (hardware pads
      nearly touch; ours are round + numbers below). *Landed as
      `KeycapButton` (ToggleButton subclass, paint-only change; param
      bindings + tooltips untouched); LED notch lights with the gate.*
  17. Keyboard section rework: **ridged** plate texture (horizontal slats),
      plates clustered **around** the center control group with staggered
      heights + tall edge plates, white round icon-button row (▽/△
      transpose + status icons), **red-ring encoder with red center**
      (ours grey), **LCD recolored blue with white text** (ours
      green-on-black), sunburst logo print, printed △ outlines at the
      plate-cluster feet; keep the digital OCT readout + SETTINGS button.
      *Landed with the print's 6|6 wave stagger (peak 3rd-from-outer edge,
      inner plates low, △ under each peak). Implementation notes: OCT
      readout moved INTO the LCD status row (its old left-edge home is
      gone); SETTINGS became a top-right corner chip; the twin/split
      dashed border marker became a short red L|R cue at the top centre
      (a full-height line would cross the centre control column); the six
      status rings bind to available telemetry (gate L/R, ext clock, step,
      offset L/R) until the backlog item nails the manual's icon set.*
- **P3 — silkscreen art. STATUS: done 2026-07-08, commits `4e25d41` (fix
  18: I/O markers) + `27b949d` (fix 19: VCO art) + `24c44d4` (fix 20: rest),
  check.sh green, every area tile-diffed vs the reference before commit.**
  As landed: VCO morph waveform glyphs on radial ticks around the big knob
  (six stations, sine→scribble; the knob moved down to the print's position
  so the ring clears the oct+3/−1 sub row) **+ numbered screw dots (1–7 VCO
  A, 8–14 VCO B — the print scatters them across the section, not around
  the knob; ours follow the print's scatter, nudged into our layout's free
  panel)**; LFO ∧/⊓ wave icons flanking the wave caption; Papa Srapa red
  rate→fm/am arrows + "LFO" dotted run + noise spark glyph (*print's S&H
  random-dot cluster skipped — our NOISE knob occupies that corner*);
  red routing arrows (pulser→clock ✓ P1, pwm-jack→pwm-knob, joystick
  knob→jack ✓ + 2nd LED); **jack direction markers panel-wide** (red ▲ =
  output, black ▸ = input, beside every jack label; JackLayer splits census
  labels on '\n' — "ext. source" went two-line like the print to keep its
  marker in-section); BP/LP response-curve icons; envelope-follower
  bell-over-burst + preamp mic glyphs; −1 sub ⊓ glyph; POWER polarity
  glyph + curled bracket.
- **M9b fidelity backlog — evaluate, then implement or document** (found by
  the eye check; each needs a manual/engine check before UI work):
  - **Headphones out behavior**: level knob → standalone monitoring gain?
    (plugin builds: decorative). Decide in M9c alongside the RT audit.
  - **Papa Srapa jack recount**: the render spec shows two hex jacks under
    rate/mod that our panel doesn't place, and `07` §1b's jack list is
    ambiguous about a distinct S&H "in" (normalled from own noise) vs our
    exposed `in` jack. Recount vs manual p8; census corrections are
    **append-only** (ids frozen).
  - **Effector center element**: the print shows a round element between
    the two 1-2-3 program switches — identify (button/window/screw) in
    manual pp21–22; add if functional.
  - **"link" control type**: we use a toggle; confirm switch-vs-jack on
    hardware and match.
  - **MUTE polarity + Init defaults**: our Init renders oscillators 4/5
    lighter on all four classic drones — confirm which visual state means
    "muted" and that Init matches hardware power-on (all five active).
  - **Keyboard status icons**: when P2 adds the icon row, wire the ones
    that are real state displays (clock, arp/seq, hold) to telemetry
    rather than printing them as decoration.
- *Verify (every phase)*: screenshot diff vs the reference PNG tiles
  (**`app/scripts/panel-shot.sh`** captures the standalone window; its
  header documents the tile-crop math vs `solar42n-panel-1.png`); all
  interactive text legible at default window size; `check.sh` green;
  user eye check (the final gate).

**M9c — RT-safety, performance, release pass** — the remainder of the
original M9 bullet above: audio-thread audit, CPU measurement, pluginval
strictness 10 + auval, MIDI clock + CC learn, signing/notarization notes.
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
