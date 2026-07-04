# Drone Lab — the intermediate Sound Creation Tool

`drone-lab/index.html` — a **standalone Web Audio synth** that does the one thing
the app can't: **stack many detuned oscillators** into a beating drone. You tune
it by ear against the Solar 42 reference, then export the result two ways.

No build, no server, no dependencies. Open the file in **Chrome** (double-click,
or `open drone-lab/index.html`). Audio starts on the first **Play** click.

## Why this tool exists
The app's `synth-osc` is a single oscillator and can't be auditioned in the Sound
Designer (see [`03-engine-gap-analysis.md`](03-engine-gap-analysis.md)). The Drone
Lab is a throwaway rig that provides the missing oscillator-stacking + microtonal
detune + per-voice drift, so a human can *find the sound by hand* and capture it.

## What it does (signal chain)
```
[intervals] → each = unison stack of N detuned oscillators (saw/tri/sqr/sine)
            → per-osc stereo pan (audition only) + per-voice slow pitch drift
   + noise (slow S&H)  →  tanh drive  →  resonant LP filter (12/24 dB) + cutoff LFO
            →  delay + chorus + convolution reverb  →  soft limiter  →  out
```
It mirrors the app's own conventions where they exist (tanh drive curve,
decaying-noise reverb impulse like `previewEngine.makeImpulse`) so what you hear
is close to what the app will do to the baked loop.

## Controls (grouped as on screen)
- **Oscillator stack** — chord root, oscillators/voice (1–7), waveform, detune
  spread (¢), stereo spread, "uneven" (microtonal), optional sub voice.
- **Background chord** — preset chips or type semitone offsets (fractions =
  microtonal, e.g. `0, 7, 14.55`); noise level. This is what the **Play** button
  holds.
- **Polivoks filter** — slope, cutoff, resonance, drive.
- **Movement** — cutoff LFO rate/depth, pitch-drift rate/depth (per-voice random phase).
- **Space** — reverb mix/decay, delay mix/time, chorus depth.
- **Amp** — attack/release, and **loop length** (export only).

Continuous knobs update **live**; structural changes (osc count, waveform, chord,
root, filter slope, reverb decay) **re-trigger** the drone automatically. The
filter, movement and space sections are a **shared bus** — they process both the
background chord and the keyboard notes together.

## Play it live (the keyboard) — the Solar 42 use case
The tool is a **polyphonic instrument**: the filter + FX chain is persistent and
every note spawns its own detuned-oscillator stack.
- **▶︎ Drone (hold chord)** holds the background bed (the chord above).
- **🎹 keyboard** plays notes *on top* — computer keys `A S D F G H J K` (white),
  `W E T Y U` (black), `Z`/`X` shift octave; or click the on-screen keys. Hold
  several at once. "All notes off" panics.
- This is exactly the Solar 42 idea: **hold a background drone and play a melody
  over it**, hearing the drone move. Keyboard notes use a snappier attack (≤0.4 s)
  so melodies speak; the chord uses the full Attack for a slow swell.
- Audio starts on the first click/keypress (browser gesture requirement).

## Step-by-step: reaching a Solar 42 drone
1. **Play.** Start from the defaults (5 saws, ±14¢ uneven, LP12 @ 900 Hz, res 55%,
   drive 35%, reverb 40%). You should already hear a moving bed.
2. **Get the beating right.** Nudge **detune spread** 8→20¢ until the churn feels
   alive but not seasick. Toggle **uneven** on for the eerie microtonal richness.
3. **Shape the cluster.** Try the **Cinematic** (`0,7,12`) then **Eerie**
   (`0,7,14.55`) chords. Add the **sub voice** for weight.
4. **Dial the Polivoks edge.** Raise **resonance** until a vocal formant "sings"
   over the drone, then back off ~10%. Add **drive** for warmth/harmonics. Set
   **cutoff** low enough to be a bed (600–1200 Hz) — melodies will sit above it.
5. **Add glacial motion.** Cutoff LFO ~0.05 Hz, small depth; pitch drift ±5–8¢.
   These ride *on top of* the intrinsic beating — keep them subtle.
6. **Space it.** Reverb 35–50%, decay 3–5 s; a touch of delay + chorus for width.
7. **A/B** against [SOUND DEMO 3](https://youtu.be/dzU8T2lKUhs). Same *class* of
   churning, resonant, cinematic drone? Log the verdict in `00-LOG.md`.

## Export
- **Export WAV loop** → renders offline, bakes a **seamless mono loop** (equal-
  power crossfade at the seam), normalized to −1 dBFS, **root C2 recommended**.
  Saved as `Solar42Drone_<root>_<len>s.wav`. Move it into `renders/`.
  - *Mono on purpose:* the app sums samples to mono on load; width is re-added by
    the app's chorus + stereo reverb.
- **Export patch JSON** → `Solar42Drone_<root>.patch.json` containing: the raw
  Drone-Lab params, a **`synthOscRecipe`** (unison blueprint for the future
  native engine — see [`05-native-extension-proposal.md`](05-native-extension-proposal.md)),
  and a ready **`samplerInstrumentJson`** for the baked loop. Move it into `patches/`.

## Installing the baked loop as a playable instrument
The exported `samplerInstrumentJson` is a valid `synth-sampler` definition. To
play it in the app **today** without touching the repo:
1. Make a pack dir in the **user library**:
   `~/Library/Application Support/ToneMatrixSynth/instruments/Solar42-Drone/`
2. Put the `.wav` there and save the `samplerInstrumentJson` as `instrument.json`
   (set `samples[0].file` to the wav's filename; `note` = the render root, e.g. `C2`).
3. Relaunch the app → the instrument appears in the user library. Add a track
   with it, hold a chord (long release + reverb bridge the per-beat gate), and
   play a melody on a **separate** track on top.

> Validate the JSON against `src/shared/instrumentJson.ts` / `docs/SOUND-DATA-MODEL.md`
> before relying on it — the exporter targets that schema but the app's parser is
> the source of truth.

## Known limits (by design)
- The oscillator movement is **baked** into the loop; the app adds live filter/
  LFO/reverb motion but can't re-tune the detune per note.
- One sample is **pitch-shifted** across the keyboard → play within ~1 octave of
  the render root for the truest beat rate. No multisampling (Sound Designer
  authors `samples[0]` only).
- Stereo width in the WAV is discarded on load (mono) — width comes from app FX.
