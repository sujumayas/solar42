# LOG — Solar 42 drone investigation

Running log per the house methodology (`docs/CLASSIC_SOUNDS_INVESTIGATION.md`).
Newest entries at the bottom.

---

### 2026-07-02 — Research + gap analysis
- Identified the target: **Elta Music SOLAR 42** hardware (analogue microtonal
  ambient drone machine). Reference clip = *SOUND DEMO 3* (Elta's YouTube).
- Sourced the architecture (Elta spec + press): 6 drone voices × ~5 detuned
  oscillators, 2 melodic VCOs, noise/S&H, 5 LFOs, dual 12 dB Polivoks filter,
  stereo path, dual cartridge FX. See `01-architecture.md`, `sources.md`.
- **Key insight:** the movement is *beating between many detuned oscillators*,
  not a single LFO. Recorded the recipe in `02-drone-recipe.md`.
- Mapped our engine (two code explorations). Verdict: **no native real-time path
  today** — no oscillator unison, `synth-osc` can't be previewed/edited,
  sustained grid notes aren't wired, mods are global not per-voice. Full detail +
  file refs in `03-engine-gap-analysis.md`.
- **Decision (user-approved):** build an intermediate **Drone Lab** (stacked-osc
  Web Audio synth) → bake a seamless loop → play via `synth-sampler` now
  ("sample now"), and keep the exported params as the blueprint for a future
  native unison engine ("recipe for later", `05-native-extension-proposal.md`).

### 2026-07-02 — Built Drone Lab prototype
- `drone-lab/index.html`: N voices × 1–7 detuned oscillators, microtonal/uneven
  detune, per-osc stereo pan, per-voice pitch drift, noise+S&H, tanh drive,
  resonant LP 12/24 dB + cutoff LFO, delay/chorus/convolution reverb, soft
  limiter, live scope. Export = seamless mono WAV loop (crossfaded seam) +
  patch JSON (raw params + synth-osc recipe + sampler instrument.json).
- How-to in `04-drone-lab.md`.

### 2026-07-02 — Drone Lab: added a playable keyboard
- Refactored the audio graph: the Polivoks filter + delay/chorus/reverb + master
  are now a **persistent engine**; each note (chord or keyboard) spawns its own
  detuned-oscillator voice into it. Filter/movement/space are a shared bus over
  both, so keyboard melodies get the same treatment as the drone.
- Added an on-screen **piano** (13 keys, 1 octave) mapped to `A S D F G H J K` /
  `W E T Y U`, with `Z`/`X` octave shift and "All notes off". Polyphonic; mouse-
  clickable. Background **Drone** button + keyboard = Solar 42's "hold a drone,
  play a melody on top."
- Also bounced the headless example with per-oscillator phase offsets → loop seam
  jump 0.004 (vs 0.14 typical in-loop) = genuinely seamless. Installed at
  `~/Library/Application Support/ToneMatrixSynth/instruments/solar42-drone/`.
- JS syntax-verified via `node --check`. Not yet auditioned in a browser here
  (Chrome extension wasn't connected) — open `drone-lab/index.html` to play.

### 2026-07-03 — official manuals acquired + digested
- Downloaded the **official Elta manuals** (Solar 42 / 42F / 42N, Solar 50,
  cartridge DIY kit + catalog, keyboard quick-ref) into `reference-docs/`;
  extracted searchable text + block-diagram PNG. Links in `sources.md`.
- Wrote `06-manual-digest.md` — verified architecture reference. **Corrections
  vs press-based notes:** drone section = 4 "classic" voices (1,2,6,7) × 5
  independently-tuned saws (staggered C0–D7 ranges, per-osc MUTE, photo-sensor
  MOD, VOLT transpose→cross-FM) + 2 Papa Srapa voices (3,8; dual
  Schmitt-trigger osc, FM/AM modes, noise→S&H) + 2 AS3340 VCO voices (4,5;
  morphing waves, VCO4→VCO5 FM normal, ADSR+loop). Keyboard *has* an optional
  quantiser (19 preset scales) — microtonal only when disabled.
- **Effector platform identified: Spin FV-1** (per cartridge DIY kit doc) —
  cartridges are EEPROMs with 3 programs; full X/Y/Z map for all 13 cartridges
  captured in the digest. FX modelling can lean on public FV-1/SpinCAD art.
- Impact on Drone Lab: needs per-osc coarse tune (interval clusters, not just
  cents unison), drift default→0 (stability is stock; movement = beating +
  MOD), pan-crossfades-filter-L/R routing, ±few-% L/R param skew for the
  "live stereo" trick. Listed in digest §"What this changes".

<!-- TEMPLATE for iterations (≤3 before flagging for human review):
### <date> — iteration N: <preset name>
- Settings: <the params you changed>
- Rendered: renders/<file>.wav
- A/B vs SOUND DEMO 3: <what matched / what didn't>
- Verdict: PASS / ITERATE / FLAG
- Next tweak & why: <...>
-->

### 2026-07-03 — project pivot: full digital 42N instrument (plan approved)
- Decided with user: build the **complete digital Solar 42N** as a native
  **JUCE 8 C++** instrument (Standalone + AU + VST3, macOS first) in `app/` —
  drone-lab stays as a reference experiment only.
- Locked trade-offs: revision **42N** · **full virtual patch cables** with
  hardware normalling (signals in volts) · effector = **fixed-point Spin FV-1
  VM** + in-house SpinASM programs approximating the cartridges · FX starter
  set CATHEDRAL/TIME/VIBROTREM/OCHRE + framework for the other 9 · custom DSP
  (PolyBLEP oscs, nonlinear ZDF Polivoks filter) · all by-ear estimates
  isolated in `TuningConstants.h` for a dedicated calibration milestone.
- Wrote `07-42n-panel-inventory.md` — 42N-specific source of truth (voice
  renumbering DRONE 1-6/VCO A-B, VCO A=sync-in / VCO B=osc-out swap, CV L→CV R
  normal, full control/jack census, normals list, unspecified-values list).
- Milestones M0–M9 tracked in-session; M0 = scaffold + this knowledge capture.

### 2026-07-03 — app M0+M1: skeleton green, first drone voice sounding
- **M0**: `app/` builds Standalone+AU+VST3 (JUCE 8.0.8 pinned, CMake, C++20);
  JUCE-free libs (dsp/fv1/engine); Catch2 suite; `scripts/check.sh` =
  configure+build+ctest+pluginval(strictness 5, SUCCESS)+render smoke.
- **M1**: `ClassicDroneVoice` — 5 free-running saws with factory ranges
  (C0-G2 … G5-D7), per-gen MUTE/MOD hooks, VOLT transpose→cross-FM dirty zone,
  AR+VCA+HOLD; 4-point B-spline polyBLEP (upgraded from 2-point after the
  alias gate measured −45 dBc audible-band; now −61 dBc, test-enforced);
  RC envelope with overshoot-clip attack + LOOP; placeholder ZDF SVF with
  L/R tolerance skew ("live stereo" from day one); APVTS params (final ID
  scheme `d1.gen3.tune`…); debug editor. 13/13 tests, gate green.
- Audition artifact: `renders/solar42n-m1-drone1.wav` (15 s: swell → beating
  cluster → VOLT dirty-zone creep; RMS/zcr profile verified). **Ear check
  pending** — open the WAV or the standalone app, tune the cluster, log verdict.

### 2026-07-03 — app M2: rack & patching core green
- `Jacks.h`: the full 42N jack census as one X-macro registry (27 outlets,
  34 inlets incl. hidden VCO A internal out) — engine, UI and state all
  consume this table; ids are frozen from here on.
- `VoltBus`: one 64-sample volt buffer per outlet, fixed process order =
  hardware causality; backward cables read the previous sub-block (~1.3 ms)
  → feedback patches behave like hardware, no double-buffering needed.
  Normals resolve automatically (incl. 42N CV L→CV R follow + Internal/Host
  kinds); cable presence = jack-switch semantics (seq ext-clock auto-switch).
- Lock-free SPSC `CommandQueue` for patch edits; patch-storm fuzz test clean.
- Mod strip live: LFO A/B (square→tri slew morph, unipolar 0..10 V), joystick
  (offset-window law), 5-step sequencer + pulser (±10 V) with stages 3/4/5,
  preamp (+40 dB, rail clip) + envelope follower (env 0-10 V, gate hysteresis).
- Debug patch-matrix UI (combo per inlet) beside the generic param list.
- 21/21 tests; pluginval SUCCESS. Patch LFO A→drone1 CV with gen MOD on =
  first *modulated* drone from the patch bus.

### 2026-07-03 — first user audition of the app (M1/M2 build) + session handoff
- **User verdict**: standalone app works; `renders/solar42n-m1-drone1.wav`
  "sounds kinda good"; **the app sounded better than the render**. No formal
  criteria yet — to be co-developed once the instrument is more complete
  (full voices + filter + FX). Verdict class: ITERATE-with-promise.
- Housekeeping for cross-session continuity: plan of record copied to
  `08-implementation-plan.md` with a live **milestone status table** (M0-M2 ✅,
  M3 next); `CLAUDE.md` created with build/run commands, engine conventions,
  and the **end-of-feature ritual** (gate green → update plan table → log →
  tasks → audition render → doc prune → local commit).
- Next session starts at **M3**: Papa Srapa voices, VCO A/B + envelopes,
  10-ch mixer with pan-as-filter-routing, nonlinear Polivoks filter, panel
  UI phase 1.

### 2026-07-04 — app M3: full voice complement + real Polivoks filters + panel UI phase 1
- **All 8 voices live.** DRONE 2/4/5 join DRONE 1 (each with its own
  tolerance-seeded gen detunes + free phases); every classic voice now has a
  working **photo-sensor** (CV jack → red LED → LDR with vactrol lag + room
  light + optional 50 Hz flicker; decision logged in 07 §4: the opto is always
  in the CV path). DRONE 3/6 = **Papa Srapa**: dual Schmitt relaxation oscs
  (non-50 % duty from the unit serial), FM (flip-flop divider chain ÷1..16),
  AM (30 µs chop slew), NOISE blend, S&H clocked from its jack — all 5
  manual modes locked by tests. **VCO A/B**: AS3340 triangle core, morph
  rotary (saw↔inv-saw zone, sine↔tri polynomial zone, square + PWM), −1 sub,
  **hard sync** (BLEP-corrected phase reset, test: locks 1.43× detune to the
  master), lin/exp CV, oct+3/low; Envelope A/B ADSR + HOLD + LOOP drive the
  VCAs into the mixer; VCO A→B CV normal = instant 2-op FM.
- **Mixer + filters**: 10-channel mixer where **PAN crossfades each channel
  between Filter L and Filter R** (constant-power; test: >20 dB separation);
  **nonlinear Polivoks ZDF-SVF** per side — asymmetric input drive, saturated
  resonance path, damping goes negative past RES 0.85 → true bounded
  self-oscillation seeded by a −180 dB noise floor (tests: no bass loss with
  resonance, ~12 dB/oct, sings at fc, silent below threshold); BP/LP per side,
  MOD L/R CV amounts, LINK, CV L→CV R normal, shared post-filter **DIST/GAIN**
  double distortion. L/R skew now also on resonance (self-osc onset differs
  per side).
- **Infrastructure**: shared 4-pt BLEP kernel extracted (`BlepKernel.h`);
  found & fixed a real bug — `railClamp` (soft tanh) was compressing CVs ~4 %
  mid-range, flattening every V/oct pitch; CV inlets now use hard `railLimit`,
  audio buses keep the musical soft clamp. ~195 APVTS params (final IDs).
- **Panel UI phase 1**: `SolarLookAndFeel` (hardware knob color code, slide
  switches, LED push buttons), `PanelLayout.h` section geometry measured off
  the render, all top-half + mod-strip sections as APVTS-attached components,
  one-transform scaling (M5 zoom-ready); patch matrix stays in the
  performance zone until the M5 cable layer.
- Gate green: 41/41 tests, pluginval SUCCESS, render smoke.
- Audition artifact: `renders/solar42n-m3-fullpath.wav` (20 s full analog
  path: D1 beating cluster L, D4 sensor-wobbled cluster R, D3 srapa chirps,
  seq→VCO A + env gate, VCO B FM pad, LFO B on filter CV, DIST warm, VOLT
  dirty-zone tail; RMS −14 dBFS, L/R corr 0.72). **Ear check pending.**

### 2026-07-04 — app M4: FV-1 effector — fixed-point VM, SpinASM assembler, 12 cartridge programs
- **Fixed-point FV-1 VM** (`fv1/Fv1Vm`): full documented ISA in S.23/int32 with
  64-bit MACs and exact saturation; coefficients decoded at real widths
  (S1.14/S1.9/S.10); PACC/LR pipeline semantics; auto-decrementing delay
  pointer over 32768 words; SIN0/1 + RMP0/1 LFOs with complete CHO
  RDA/SOF/RDAL flag semantics (REG latch, COS, COMPC/COMPA, RPTR2,
  NA trapezoid crossfade). Primary sources fetched and mined this session:
  Spin's instruction/architecture pages + AN-0001 ("Basics of the LFOs") for
  the exact rate formulas and idioms. Two non-obvious facts locked by tests:
  **positive ramp rate = pitch UP** (read pointer runs toward the incoming
  samples — first implementation froze playback to DC), and **pot ADCs are
  9-bit with hysteresis** (datasheet; the plan had guessed 10).
- **SpinASM assembler** (`fv1/SpinAsm`, ~700 LOC, runtime): labels, EQU/MEM
  (both orders, `#`/`^` modifiers, N+1 allocation), $/%/0x literals, full
  expression grammar, SKP targets, all pseudo-ops, per-width clamping with
  warnings. **Output is word-identical to asfv1 1.2.7** on the four AN-0001
  programs (committed as .hex goldens + regen script) and on all 12 in-house
  programs. `fv1asm` CLI assembles/diffs; runs any community FV-1 source.
- **12 starter cartridge programs** (`app/programs/`, authored in-house from
  the 06 X/Y/Z catalog): CATHEDRAL (shimmer with ±1-oct shifters in the tank
  loop / oct-up delay / space reverb), TIME (delay-reverb / delay-chorus /
  delay-vibrato), VIBROTREM (tremolo / vibrato / quadrature chorus), OCHRE
  (reverse one-shots long+short / free-run reverse loop). Design note: the
  one-shots are **capture-then-reverse** — the trigger parks the pointer a
  window into the future, records, then plays that window backwards (first
  cut played pre-trigger history, i.e. silence — caught by the trigger test).
- **Infrastructure**: `PolyphaseResampler` (48-tap Kaiser β=10, 256 phases,
  >90 dB round-trip SNR at 44.1/48/96k; the ~15 kHz band edge falls out of
  the 32,768 Hz chip rate); `EffectorModule` — dual VMs, channel R clock
  skewed ±0.05 % from the unit serial, POTn = knob + CV/20 with 20 ms RC,
  equal-power BLEND, slotted between DIST and master; FxCvX/Y/Z jacks live.
  Cartridge slot is **hardware-faithful**: flipping a 1-2-3 toggle loads that
  program from the currently inserted cartridge; swapping the cartridge alone
  changes nothing (UI note: pick a cartridge, then flip 1-2-3 to hear it).
  ~203 APVTS params; EffectorSection UI replaces the M4 placeholder.
- Gate green: 55/55 test cases (~523k assertions), pluginval SUCCESS, render.
- Audition artifact: `renders/solar42n-m4-shimmerpath.wav` (24 s, the M3
  scene now ending in CATHEDRAL shimmer on both channels — X=0.6 oct-up
  bloom, Z=0.7 decay, BLEND 0.45; RMS −15.4 dBFS, L/R corr 0.74; the skewed
  chips + tolerance seeds keep the tails alive in stereo). **Ear check
  pending — supersedes the M3 render check.**

### ~~<pending> — first audition + bounce~~ (superseded 2026-07-03)
The Drone Lab → sample-loop → ToneMatrixSynth bridge was superseded by the
full native Solar 42N instrument (`app/`, see `08-implementation-plan.md`).
drone-lab stays as a historical experiment only. "Human ear is the final
gate" carries over — see the Listening protocol in `CLAUDE.md`.

### 2026-07-04 — app M5: full panel UI + virtual cable layer
- **Jack census complete**: `ui/PanelLayout.h` (JUCE-free) now maps all **63
  panel jacks** to measured positions in the 4950×3200 space; a constexpr
  static_assert keeps it 1:1 with `engine/Jacks.h` forever (a registry append
  without a panel position no longer compiles). Registry census corrections
  (append-only, ids frozen): **VCO A/B DRY OUTs** (the render's red "VCO A"/
  "VCO B" jacks flanking the mixer logo — 07's "position unclear" flag
  resolved) and the **preamp ext. source jack** (overrides the host-input
  "piezo" when patched — e.g. VCO B osc → preamp → follower now works).
- **CableLayer**: press a jack → ghost cable + compatible-jack highlights;
  release to connect; grab a plug to re-route (drop in space = unplug, like
  hardware); Alt-click unplugs; one cable per inlet, unlimited per outlet
  (built-in mult); unpatched inlets show their hardware normal as a dashed
  trace on hover; bezier cables with gravity sag + plug bodies, ~85 % opacity.
  Canonical cable list = `PatchBay` (CABLES child of the APVTS tree, jacks
  stored by display name) → save/recall round-trips cables already; engine
  edits ride the M2 lock-free command queue.
- **Telemetry**: seqlock-guarded POD snapshot published per 64-sample
  sub-block (torture-tested: zero torn reads); UI samples at 30 Hz — drone
  gen LED bars, photo-sensor window glow (room light + CV), srapa modulator
  LEDs, sequencer step LED, preamp clip, follower env/gate, master peaks.
  Verified live: gating a voice from the keypad lights its bar; the seq LED
  visibly steps.
- **Editor**: fit-width default, Cmd+scroll / pinch zoom 100–300 % anchored at
  the cursor, scroll/drag-empty-space pan, double-click a section band to
  zoom to it, double-click background to fit; aspect-locked resize. The M2
  debug patch matrix is deleted. Performance zone: DRONE VOICES keypad (wired
  to the d1–d6 gate params, hardware column order) + position-locking
  joystick pad (joy.x/joy.y) + keyboard placeholder print (M6). Room-light
  knob + 50 Hz flicker switch live in the header (digital-only controls).
- Layout matched against `solar42n-panel-1.png` over three screenshot
  iterations (header jack block, rotated FILTER L/R tags, hardware-print
  font sizes, per-section knob/jack interleave).
- Gate green: 61/61 test cases, pluginval SUCCESS, render smoke.
- **No sonic change** (dry outs are silent until patched) — the M4 shimmer
  render ear check is still the pending audition. Visual artifact:
  `renders/m5-panel-screenshot.png` — **panel eye check pending**: compare
  against the reference render; patch a few cables by hand (LFO A out →
  DRONE 1 CV is a good first pull) and confirm the drag feel.
- Known M6+ leftovers: keypad buttons latch (hardware is momentary + HOLD),
  keyboard jack strip parked above the keyboard zone (render hides these),
  1-2-3 program toggles are combo boxes, no faithful cartridge-slot art yet.

### 2026-07-04 — app M6: touch keyboard + drone keypad
- **Firmware as pure JUCE-free logic** (`engine/keyboard/`, unit-tested):
  `TouchKeyboard` (behaviours single/twin/split, per-side mode
  keyboard/arp/seq, last-note priority, portamento + legato, vibrato LFO
  with delay + pressure control), `Quantiser` (19 named scales + root +
  per-note mask editor; **all notes off = microtonal**, plates hand-tunable
  in 0.0025 V steps; LOAD SCALE also retunes the 12 plates to walk the
  scale), `Arpeggiator` (Solar builds the arp from **plate numbers** not
  incoming pitch; fwd/back/ping-pong/random, HOLD latch, variation ×1–3 by
  a 1–12 semi interval, 1–8 step rhythm gate mask), `KbSequencer` (2–16
  steps, free/key-advanced run, directions incl. endpoint-repeating
  ping-pong, per-step gate/value, continuous-vs-gated CV, rhythm mask),
  `KbClock` (internal 10–300 BPM, **auto-switch to external** on a CLOCK-in
  pulse, BPM edit reverts to internal; per-consumer mult/div scaler),
  `PressureShaper` (raw / ASR / AD / looped-AD / random + rise/fall),
  `KbMenu` (encoder push-menu state machine: browse/edit + scale/rhythm/seq
  sub-editors + preset page, two-line LCD display model).
- **Rack integration**: keyboard runs FIRST in the fixed order, so its
  V/OCT + GATE L reach both VCOs / envelopes through the M2 hardware
  normals in the same sub-block; CLOCK-in / RESET-in jacks are live (arp
  syncs to a patched pulser, RESET returns it to step 1). Config crosses to
  the audio thread via a generic `SeqlockBox`; plate/button gestures are
  plain atomics (`KbTouchState`). New `TuningConstants`: vibrato min/max Hz +
  depth + delay, arp/seq gate duty (all M8-tunable).
- **UI**: playable `KeyboardSection` replaces the M5 print placeholder —
  12 plates (mouse Y in a plate = pressure = capacitive area; horizontal
  drag glissandos; Shift-click = sticky finger for chords; scroll-on-plate
  tunes it), two transpose buttons with offset LEDs + octave readout, the
  push-encoder + green LCD rendering the faithful firmware menu, gate/clock/
  step status row, and a SETTINGS button. Parallel `SettingsDrawer` (desktop
  overlay) exposes the *same* KbConfig through modern combos/sliders/toggles
  + a click-mask scale editor, a 16-step bar/gate editor and a preset rack —
  both paths write one KEYBOARD state child so display + drawer + engine
  always agree. Keyboard setup now **persists** with the plugin state
  (config incl. microtuning + presets A–D; rebinds on setStateInformation).
- Gate green: 85/85 test cases (~1.9k new assertions in `TestKeyboard.cpp`),
  pluginval SUCCESS, render smoke. The M6 verify criteria are covered by
  passing tests: glide on plates, arp locked to a patched pulser clock,
  quantiser-off microtonal plate tuning.
- Audition artifact: `renders/solar42n-m6-keyboardarp.wav` (24 s — the M4
  drone/shimmer bed with the keyboard as the melodic layer: HOLD-latched
  minor-triad arp, variation an octave up, portamento glide, clocked by the
  PATCHED pulser and normalled into the VCOs; RMS −15 dBFS, L/R corr 0.72).
  **Ear check pending.** **Panel eye check pending**: the standalone launched
  clean but the first-run macOS mic-permission prompt is a system-modal only
  the user can dismiss — click through it and look at the bottom-center
  keyboard zone (play a plate, open SETTINGS, try an arp over the sequencer
  pulser). This screenshot step carries over exactly like M5's panel eye
  check did.
- Prior M6+ leftovers status: keypad buttons still latch (toggle params —
  momentary + HOLD nuance is cosmetic, deferred); keyboard jack strip stays
  parked above the zone (the render hides them); 1-2-3 program toggles +
  cartridge-slot art remain M7 polish.

### 2026-07-04 — app M7: state, presets, conveniences
- **Full save/recall round-trip.** Two new state children join CABLES/KEYBOARD:
  `CARTRIDGES` persists what each FV-1 chip actually HOLDS — the engine's
  `EffectorModule` now takes explicit loaded-slot params (flip a 1-2-3 toggle
  = latch that program from the inserted cartridge; swapping the cartridge
  alone changes nothing, and a saved mid-swap slot restores exactly), and
  `TOLERANCES` persists the unit serial (restore = same "hardware unit";
  changing it suspends, reseeds and re-prepares the rack). The editor's size
  rides a `UI` child. Pre-M7 states still load (loaded slots derive from the
  panel, serial keeps the current unit).
- **Presets.** `PresetManager` + a preset bar strip above the panel (outside
  the skeuomorphic surface, per plan): 6 code-built factory presets — Init,
  Cathedral Bloom (the M4 bed), Pulser Arp (the M6 scene), Srapa Aviary
  (S&H-stepped filter + TIME), Sensor Swell (LFO-on-LED in a dark room +
  VIBROTREM), Reverse Air (OCHRE free-run loop) — plus user `.s42n` snapshots
  in ~/Library/Application Support/Solar42N/Presets, prev/next steppers, and
  a Swap Unit action (new tolerance serial, same patch). Loading a preset
  keeps YOUR unit serial — a patch always sounds like your unit.
- **Click-free switching.** Preset loads fade the output (~12 ms), apply on
  the message thread once the fade has *measurably* landed (wall-clock timers
  proved unreliable under a starved message loop — the apply gates on actual
  fade progress, with an idle-transport fast path and a 250 ms hard cap),
  then fade back. Verified by test: the swap dips to true digital silence
  with no hard sample step.
- **Conveniences.** Every jack now explains itself on hover from the frozen
  registry (direction, voltage range, which normal a cable breaks); curated
  tooltips on the non-obvious controls (VOLT's dirty zone, PAN-as-filter-
  routing, slot semantics, 9-bit pots...). Automation naming pass: parameters
  grouped per panel section, DAW lanes and knob bubbles show real units
  (s / Hz / dB / V / % / L-R), double-click returns a knob to default. The
  effector's 1-2-3 combos became real 3-position slide switches in a drawn
  cartridge bay ("flip 1-2-3 to load"). Room-light control already existed
  (M5) — confirmed persisted like everything else.
- **Verification.** New `solar42n_statecheck` harness (ctest + check.sh, runs
  the REAL processor): randomized-rig round-trip → every param + child
  restored and two relaunched instances render sample-identical audio (the
  honest reading of "DAW kill/relaunch → identical sound": free-running
  phases can't survive a kill, so relaunch-vs-relaunch is the guarantee);
  all six factory presets load through the faded path and render finite,
  audible sound; the click-free bound above. Gate: 87/87 (86 Catch2 cases
  incl. the new effector slot-semantics test + statecheck), pluginval
  SUCCESS, render smoke.
- Audition artifacts (new sounds the user hasn't heard yet):
  `renders/solar42n-m7-preset-srapa-aviary.wav` (20 s — both Papa Srapas as
  an FM/AM bird pair, DRONE 3's S&H stepping filter L off the pulser, TIME
  delay-reverb) and `renders/solar42n-m7-preset-reverse-air.wav` (20 s —
  sparse two-cluster pads through OCHRE's free-run reverse loop).
  **Ear check pending.** Panel eye check (M5/M6) still carries over — the
  preset bar + cartridge bay + tooltips are new on-screen items to glance at.
