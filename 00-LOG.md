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

### <pending> — first audition + bounce
- [ ] Open Drone Lab in Chrome, confirm it plays and the scope moves.
- [ ] Tune toward SOUND DEMO 3; export a loop to `renders/`.
- [ ] Install as `synth-sampler` in the user library; play in a song with a
      melody track on top.
- [ ] A/B verdict logged here. Human ear is the final gate.
