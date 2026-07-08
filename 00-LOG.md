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

### 2026-07-05 — app M8 kickoff: calibration kit + preset durability + two bug fixes
- **Preset durability (user request: "presets must survive rebuilds").**
  Two-part answer: (1) location — `PresetManager` was resolving user presets
  to `~/Library/Solar42N/Presets` (JUCE's `userApplicationDataDirectory` is
  `~/Library` on macOS, not Application Support); now presets live in
  `~/Library/Application Support/Solar42N/Presets` and anything in the legacy
  dir is auto-migrated on launch (the user's "Testing Preset.s42n" moved over
  intact — presets were never inside the build tree, so rebuilds never
  touched them). (2) format — `applyState()` now merges every loaded state
  over the pristine default tree: params missing from an older build's
  preset come back at their *defaults* (not stale values), params from a
  newer build are dropped, and saves stamp a `stateVersion` for future
  migrations. Covered by a new statecheck test (doctored older/newer blobs).
- **Bug: cables died on re-prepare.** `Rack::prepare()` wiped the VoltBus
  routing table, so every host `prepareToPlay` (transport start, buffer/device
  change) silently unplugged all cables in the engine while the UI kept
  drawing them. Found because the calibration renders patch then re-prepare —
  the pulser scene came back silent. Fix: `VoltBus::clearBuffers()` re-inits
  signal state but keeps `patched_` ("power-cycling the rack must not pull
  patch cords"); regression test in `TestRouting.cpp`.
- **Bug: standalone killed by macOS TCC.** Crash report 2026-07-04 22:22:
  the JUCE MIDI-device scan touches Bluetooth and the Info.plist had no
  `NSBluetoothAlwaysUsageDescription` → SIGABRT on launch day. Added
  `BLUETOOTH_PERMISSION_ENABLED/TEXT` to `juce_add_plugin`.
- **M8 calibration kit.** New `solar42n_calib` console tool renders 11
  focused audition scenes through the REAL processor (real knob→physical
  maps), one per tunable domain of `TuningConstants.h`: drone/VCO envelope
  ranges, LFO A/B × range switches, pulser span, VOLT dirty zone, filter
  range/self-osc/scream, double distortion, photo-sensor lag + flicker,
  Papa Srapa spans, pan law/L-R skew, per-cartridge full-wet FX voicing.
  Each print-out is a timeline sheet; `09-calibration-protocol.md` is the
  new listening protocol (scene → constants → verdict table, A/B criteria
  vs SOUND DEMO 3, freeze rules). Renders in `renders/calib/` (~5 min).
  Scenes verified by segment-RMS/brightness analysis (e.g. LFO wobble 17×
  brightness swing at 5 Hz, OCHRE one-shot fires twice, pan law flat).
- Analytical pass over `TuningConstants.h` vs the 07 voltage table: no
  contradictions found; all values stay as-authored pending ear verdicts.
- **Ear check pending (the M8 loop proper):** listen through
  `renders/calib/` with `09-calibration-protocol.md` open, give per-scene
  PASS/ITERATE verdicts (cite SOUND DEMO 3 / video moments where possible).

### 2026-07-05 — M9 re-sliced with the user: MIDI-in pulled forward + full-panel UI review
- **Why**: the user has no hardware Solar 42 available right now, so the M8
  ear loop (A/B vs the real machine's character) pauses; instead we pull the
  **digital-only** work forward. M9 split into M9a (MIDI-in), M9b (UI pass),
  M9c (original RT/perf/release scope). Full locked specs live in the **“M9
  breakdown”** section of `08-implementation-plan.md` — written and committed
  *before* implementation at the user's request (session time may run out;
  the repo is the durable tracker).
- **M9a decisions (user):** C1–F1 (MIDI 24–29) gate drone voices 1–6 on any
  channel; C2 and up play the 12 plates with **per-note octave shift**
  (60–71 = native tuning) so a MIDI keyboard reaches octaves without moving
  the panel OCT buttons — deliberately beyond hardware; velocity/aftertouch →
  plate pressure; **drone gating is a persisted checkbox: momentary OR
  toggle-HOLD** (user explicitly wants both workflows); CC64 sustain. Target
  controller: Arturia KeyLab 37.
- **UI review (screenshots vs `reference-docs/solar42n-panel-1.png`):**
  confirmed the user's two complaints — illegible small text (cartridge
  selector + the combo boxes that are physical switches/knobs on hardware:
  DIVIDER, LFO x1/x6/x10, STAGES) and overlapping controls (envelope A/B
  hold/loop clipping, drone-section bottom jack labels on the border, S&H
  box collisions, effector CV-jack/knob crowding, undersized filter-strip
  knobs). Fidelity gaps: DRONE VOICES pads should be square, keyboard
  section should cluster plates around the center encoder/LCD group.
  Prioritized P1/P2/P3 list in the M9b spec.

### 2026-07-05 — app M9a: MIDI-in — play the plates + drone voices from a keyboard
- **What landed.** `s42::MidiPerformer` (`engine/keyboard/MidiPerformer.h`,
  JUCE-free, header-only): held-MIDI-note state → the exact gestures the
  panel produces. Notes 24–29 (C1–F1) gate drone voices 1–6 (omni); notes
  36+ play the 12 plates (plate = note mod 12, C4–B4 = native tuning, each
  keyboard octave away = ±1 V per-touch shift — the digital-only octave
  reach, panel OCT untouched); velocity → pressure (floored at 0.05 so pp
  notes cross the touch threshold); channel/poly aftertouch ride above the
  velocity while held; CC64 sustains plates + momentary drone gates; CC123/
  120 = panic. `KbTouch` grew `plateShift[12]` and `TouchKeyboard` applies
  it pre-quantise in all three note paths (keyboard/arp/seq), so quantiser,
  microtuning, twin/split, arp, seq and pressure modes all work from MIDI
  exactly as from fingers.
- **Drone gate checkbox (user decision).** APVTS param `midi.droneLatch`
  (persisted, automatable; checkbox in the settings drawer under MIDI):
  off = momentary gates like the DRONE VOICES keypad, on = each note-on
  flips that voice's real `dN.hold` parameter — flipped on the message
  thread via a 30 Hz processor timer draining atomic toggle counters, so
  the panel HOLD buttons, host automation and MIDI share ONE latch state
  (and the audio thread stays allocation-free).
- **Seam.** All parsing in `processBlock`'s `MidiBuffer` — the JUCE
  standalone wrapper feeds device MIDI through the same buffer, so
  Standalone + AU + VST3 share one handler. MIDI plate touches merge with
  mouse touches by max-pressure; a ringing MIDI note owns its plate's
  octave shift (last-pressed wins on collisions).
- **Sonic change: none by construction** — the adapter reuses the existing
  touch path (all 25 pre-existing keyboard tests pass untouched), so no new
  audition WAV; the audition IS the user playing their Arturia KeyLab 37
  into the standalone (enable the device in Options → Audio/MIDI Settings).
  **Hands-on verdict pending.**
- Gate: 94/94 tests (+6 MIDI cases: plate map/octave shift/collision,
  velocity floor + AT, momentary + sustain, latch counters, guard band,
  V/OCT shift through the real firmware), pluginval SUCCESS, render smoke.

### 2026-07-05 — app M9b P1: legibility & overlap pass (screenshot-verified)
- **Root cause + fix.** All print is in logical panel units (1 unit =
  0.1 mm) under one scale transform; several labels were authored well below
  the hardware's ~3 mm lettering, so at a normal window (~31 % scale) they
  vanished. Introduced a legibility floor: LabeledKnob/ChoiceBox label fonts
  clamp to ≥ 26 units, combo text scales with box height (cap 48), push-
  button text caps circle size so the label keeps room.
- **Dropdowns → the hardware's physical controls.** New `ChoiceSlideSwitch`
  (vertical N-detent switch for choice params, position labels in hardware
  top-down order): LFO A/B range now a real **x6 / x1 / x10** switch (x1
  centre), sequencer STAGES a **4 / 5 / 3** switch (5 centre). Papa Srapa
  **DIVIDER is now a blue knob** (stepped, value bubble shows 1/2/4/8/16).
  Cartridge selector ~2.5× bigger with legible CARTRIDGE / "flip 1-2-3 to
  load" captions; the 1-2-3 program switches got readable in-component
  digits (they were drawn cream-on-cream and half outside the component).
- **Overlaps fixed** (all screenshot-verified before/after): jack labels
  tucked closer under the nut so bottom-row GATE/env/env out/in-clock-out
  no longer sit half-cut on section borders; envelope sections rebuilt
  (hold/loop clear of the title band, ADSR raised, jack labels flipped
  ABOVE the nuts via a new census `labelAbove` flag — those jacks have only
  49 units below); effector X/Y/Z moved below their CV jacks' labels;
  filter strip inset so the rotated FILTER L/FILTER R tags stop colliding
  with the FREQ knobs, BP/LP marks added at each mode switch, knobs
  enlarged; Papa Srapa voice badge floated off the S&H box + out jack;
  pulser→clock arrow no longer crosses the pulser knob; LFO activity LED
  off the out-jack label; "HOLD" no longer truncates to "HO…".
- Eye-check artifact: `renders/m9b-p1-panel-screenshot.png` (vs
  `m5-panel-screenshot.png` and the reference PNG). **User eye check
  pending.** P2 (square DRONE VOICES pads, keyboard-section fidelity
  rework) and P3 (silkscreen glyphs) remain, specs in 08.

### 2026-07-07 — M9b P1 eye check (verdict: ITERATE) + P1.5 fidelity plan
- **Method.** Cropped `renders/m9b-p1-panel-screenshot.png` and the reference
  `reference-docs/solar42n-panel-1.png` into matched section tiles (sips) and
  compared every section side by side at readable zoom; cross-checked each
  suspected gap against `07-42n-panel-inventory.md` and the manual text
  before calling it real.
- **P1 verdict: the legibility pass held** — floor fonts, slide switches,
  DIVIDER knob, cartridge bay, envelope/filter overlap fixes all confirmed
  in the screenshot. But the tile review surfaced a second wave of fidelity
  gaps → scoped as **M9b P1.5 (15 fixes, detailed where/what/how/verify in
  08)** so nothing lands unreviewed or breaks layout invariants.
- **Headline finds:**
  - Classic drone sections draw **MOD/TUNE/MUTE top-to-bottom; hardware
    prints MUTE/TUNE/MOD** (numbers 1–5 at top). `07` §1a had the same
    error — the doc is now corrected (render spec + manual print agree).
  - **VCO pwm/pw knob captions are invisible** — the caption strip sits
    under the census jack-row nuts.
  - **Headphones out (jack + level, inventory §1g) was never drawn** —
    behavior decision (standalone monitor gain?) parked in the backlog.
  - Mixer print details (PAN◆/L◄►R marks, black channel band, EXT.AUDIO),
    MOD L/R knobs belong above the filter strip not in it, sensor windows
    render pink at idle (hardware: white), missing СОЛАР 42N print, SOLAR
    42ᴺ lockup colors/superscript, ĀMBIĒNT diacritics, seq voltage-ruler
    stripe, S&H corner badge lost in P1.
  - P2 enriched: keypad pads square w/ numbers above + LED notch; keyboard
    LCD must be **blue** (ours green), encoder **red-ring**, ridged
    staggered plates. P3 enriched: morph position dots 1–7/8–14, jack
    direction markers (red ▲ out / black ▸ in) panel-wide.
  - Backlog (evaluate before building): Papa Srapa jack recount vs manual
    p8 (render shows two unplaced hex jacks; census append-only), effector
    center element between 1-2-3 switches, "link" control type, MUTE
    indicator polarity + Init defaults, keyboard status icons → telemetry.
- No code changed; docs only (07 correction, 08 plan, this entry). Next
  session: implement P1.5 groups in order, one commit per group, tile-diff
  each against the reference before moving on.

### 2026-07-07 — app M9b P1.5: fidelity corrections landed (screenshot-verified)
- All 15 fix groups from the morning's eye-check plan implemented, one
  commit per group (`58942b4`…`60ebf31`), each rebuilt + tile-diffed
  against the same reference crop before moving on:
  1/12/15. Classic drones now match the print: **MUTE/TUNE/MOD** rows
  top-to-bottom with column numbers on top, rotated edge markers, **no
  title band** (print has none — rows use the full height), LED bar as
  red vertical segments. DRONE 3/6 + VCO A/B wear short top-right title
  chips, envelopes top-left, DUAL EFFECTOR a centred chip; S&H box got
  the print's corner badge and stopped striking through in/clock/out.
  2/13. VCO pwm/pw captions freed from under the jack row; "low" printed
  under oct+3.
  3. Mixer print: PAN ◆ + L◄ ►R marks, black channel band (EXT.AUDIO),
  VOL captions, separators; VOICE MIXER moved to the hardware's center
  badge between the envelopes.
  4. Effector/filter to print layout: MOD L/R black knobs above the
  CV L/R jacks, strip re-slotted FREQ RES · CV L DIST link GAIN CV R ·
  FREQ RES around the frozen jack fractions, BP/LP dotted bracket +
  floating toggle, **headphones glyph/jack/level silkscreen** (behavior
  = M9c decision), BLEND/MASTER VOLUME captions above.
  5. Photo-sensor windows **white at idle** — telemetry now carries the
  red LED's own vactrol-lagged glow (`PhotoSensor::ledGlow`), room light
  no longer tints the window pink. Audio path untouched.
  6. Envelope jack labels moved BELOW the nuts (row raised to fy 0.72,
  ids frozen) — the P1 label-above compromise is gone.
  7/8/9. Seq: striped voltage-ruler band print + cv-over-gate stacked at
  the right edge; census grew a labelSide (below/above/left) field for
  the print's beside-the-nut labels. Joystick X/Y outs stacked centre +
  LED. Preamp gain to the print's medium knob.
  10/11/14. SOLAR 42ᴺ lockup (red 42, black superscript N), ȦMBIĒNT
  diacritics, СОЛАР 42N print, WET OUT bracket, EXT. AUDIO beside its
  jack, dashed box marks the digital-only ROOM LIGHT/50 Hz corner,
  KEYBOARD CV jack strip boxed, d-pad corner dots.
- Gate: `check.sh` ALL GREEN — 94/94 tests, pluginval SUCCESS, render
  smoke. **No sonic change** (only telemetry semantics changed).
- Eye-check artifact: `renders/m9b-p15-panel-screenshot.png` — **user eye
  check pending**. Next: M9b P2 (square keypad pads + keyboard rework,
  blue LCD, red-ring encoder), P3 glyphs, backlog verifications.

### 2026-07-08 — app M9b P2: keypad + keyboard to the print (screenshot-verified)
- Both P2 fix groups landed, one commit each, each rebuilt + tile-diffed
  against the matching `solar42n-panel-1.png` crop before commit:
  16. **DRONE VOICES keypad** (`23dacb5`): six round PushButtons became
      square keycaps — dark shadow base, bevelled top face that rides
      up-left until pressed, light LED notch at the top centre (lit =
      gate on), voice numbers printed above the pads, tighter column
      pitch. Param bindings/tooltips untouched (`KeycapButton` is a
      paint-only ToggleButton subclass).
  17. **Keyboard section** (`e5fa794`): plates got the print's ridged
      slat texture and now cluster **6|6 around a centre control group**
      in the wave stagger (peak 3rd-from-outer, inner plates low, printed
      △ outlines under each peak plate's foot). Centre group per print:
      white status-ring icon row (bound to gate L/R, ext clock, step,
      offset L/R telemetry for now — manual icon meanings stay a backlog
      item), big white ▽/△ transpose buttons flanking a grey-bezel
      **red-centre push encoder**, **LCD recolored hardware blue** with
      white text, sunburst logo print. Kept digital divergences: OCT
      readout moved into the LCD status row, SETTINGS chip top-right;
      twin/split border marker is now a short red L|R cue at top centre.
      Interaction model (glissando, pressure, sticky plates, encoder
      detents/long-press) and all bindings unchanged — geometry + paint
      only.
- Gate: `check.sh` ALL GREEN — tests, pluginval SUCCESS, render smoke.
  **No sonic change.**
- Eye-check artifact: `renders/m9b-p2-panel-screenshot.png` — **user eye
  check pending** (covers P1.5 + P2). Next: M9b P3 silkscreen glyphs
  (morph position dots, jack direction markers panel-wide, wave/curve
  icons), then the fidelity backlog verifications.

### 2026-07-08 — app M9b P3: silkscreen art pass (screenshot-verified)
- All three P3 fix groups landed, one commit each, each rebuilt +
  tile-diffed against the matching `solar42n-panel-1.png` crop first:
  18. **Jack I/O markers panel-wide** (`4e25d41`): the print's direction
      language — black ▸ = input, red ▲ = output — now prints beside every
      census jack label (all three labelSide variants), colour-matched to
      the existing red-out/ink-in label code.
  19. **VCO silkscreen** (`27b949d`): six morph waveform glyphs
      (sine→triangle→saw→pulse→crossed→scribble) on radial ticks around
      the big knob — the knob dropped to the print's lower position so the
      ring clears the oct+3/−1 sub switch row; numbered screw dots
      scattered per the print (1–7 VCO A, 8–14 VCO B; the plan said
      "around the knob" but the print scatters them — print wins); red
      routing arrow pwm-jack→pwm-depth-knob; ⊓ glyph after "−1 sub".
  20. **Mod strip + srapa + filter + header** (`24c44d4`): LFO ∧/⊓ icons
      flanking "wave"; srapa red rate→fm/am arrows + "LFO" dotted run +
      noise spark glyph; joystick curved knob→jack arrows + second LED;
      BP hump / LP shelf curve icons; preamp mic glyph ("ext. source"
      went two-line like the print so its marker stays in-section —
      JackLayer now splits labels on newline); envelope-follower red
      bell over black burst; POWER 12V DC polarity glyph + curled bracket.
- Documented divergence: the print's S&H random-dot cluster (drone 3/6
  right edge) has no home — our NOISE knob owns that corner. Revisit only
  if the Papa Srapa jack recount (backlog) reshuffles that region.
- Gate: `check.sh` ALL GREEN — 94/94 tests, pluginval SUCCESS, render
  smoke. **No sonic change** (paint + one knob-position shift only).
- Eye-check artifact: `renders/m9b-p3-panel-screenshot.png` — **user eye
  check pending** (covers P1.5 + P2 + P3; note the screenshot carries the
  standalone's saved patch cables, not part of the print). Next: the M9b
  fidelity backlog verifications (srapa jack recount, effector centre
  element, link control type, MUTE polarity, keyboard status icons,
  headphones behavior in M9c), then M9c.

### 2026-07-08 — app M9b P4: fidelity backlog resolved — M9b complete (checks pending)
- Research pass first (manual txt + official PDF pp21–22 + Solar 42 manual
  + render-spec crops), resolutions written into 08 before code; then one
  commit per fix:
  21. **Papa Srapa jack recount** (`c676d65`): the print shows 7 jacks per
      srapa voice — our census had 6. The missing one is the **cv input**
      ("▲cv" under the mod/divider gap): appended `D3CvIn`/`D6CvIn` (enum
      tail, ids frozen) and wired into the audio oscillator's pitch node
      (law unspecified → `kSrapaCvOctPerVolt = 0.5`, by ear; audio-rate
      capable). The modulator cv OUT moved to the print's rate/mod gap
      with the print's marker-only legend; activity LED rides along.
      Manual p8 confirmed the distinct S&H "in" we already model. 07 §1b
      corrected; new unit test (+4 V ≈ +2 oct).
  22. **link is a push button** (`def5722`): manual says "when switched
      on", print draws the flat black circle (same art as the effector's
      button, unlike any knob/toggle). SlideSwitch → round PushButton,
      same `filt.link` param; cream LED carries the latch state.
  23. **Effector centre element identified** (`a6cd35f`): it's the Solar
      42's **reset/load button** (42 manual: "press reset/load button …
      L/R FX"; the 42N auto-loads on toggle flip so its text dropped it,
      but the panel kept one unlabelled button). Ours latches the
      INSERTED cartridge + current 1-2-3 programs into both chips — the
      only way to load a swapped cartridge without flipping a toggle.
      New BayButton → PanelView callback →
      `Solar42NProcessor::reloadCartridgeSlots()`. The "flip 1-2-3 to
      load" caption retired per the print (tooltip carries the hint).
  24. **Init mute defaults** (`6261239`): polarity was right all along
      (buttons carry no state; the OSC STATUS badge does, lit = running) —
      the bug was the parameter default `g >= 3` muting gens 4/5 on all
      four classic drones. Now all five run at power-on like hardware.
      **SONIC CHANGE on Init**: denser drones, gens 4/5 add near-unison
      beating. Audition: `renders/solar42n-m9b-p4-init-all-gens.wav`.
  25. **Keyboard icon rings rebound** (`c978bea`): the print's icon row IS
      the hardware keyboard's I/O jack row in the manual's IN-OUT order
      (clock 🕐 / v/oct ⚡ / gate L ⊓ | gate R ⊓ / pressure ↓ / reset ⏎ —
      the P2 glyphs already matched). Rings now glow with the real
      signals; new `kbReset` telemetry (150 ms decay) keeps edge pulses
      visible at 30 Hz.
- Deliberately unresolved: **headphones out behavior** stays an M9c
  decision (RT audit decides standalone monitoring gain).
- Gate: `check.sh` ALL GREEN — 95/95 tests, pluginval SUCCESS, render
  smoke.
- Eye-check artifact: `renders/m9b-p4-panel-screenshot.png` (standalone
  carries its saved patch cables, not part of the print). Ear-check
  artifact: `renders/solar42n-m9b-p4-init-all-gens.wav` (Init density
  change). **User eye + ear checks pending — the final M9b gate.**
  Next: M9c (RT-safety, performance, release pass).
