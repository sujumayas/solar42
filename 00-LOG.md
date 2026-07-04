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

### ~~<pending> — first audition + bounce~~ (superseded 2026-07-03)
The Drone Lab → sample-loop → ToneMatrixSynth bridge was superseded by the
full native Solar 42N instrument (`app/`, see `08-implementation-plan.md`).
drone-lab stays as a historical experiment only. "Human ear is the final
gate" carries over — see the Listening protocol in `CLAUDE.md`.
