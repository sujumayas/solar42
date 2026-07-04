# Engine gap analysis — can Tone Matrix Synth make a Solar 42 drone?

Findings from two code explorations of the Sound Designer + both audio engines
(native CoreAudio and the Web Audio preview). File references are to the repo at
investigation time.

## TL;DR
**Not in real time, today.** The heart of the Solar 42 sound — *many detuned
oscillators beating together, held indefinitely* — hits three hard engine limits.
**But** the app can play a rich **looping sample** with a good filter, LFOs and
reverb. So we synthesize the drone **outside** the app (Drone Lab), bake it to a
seamless loop, and play it back as a `synth-sampler`. That works with **zero**
changes to the app.

## What blocks a native real-time Solar 42 (most impactful first)

1. **No oscillator stacking / unison.** `synth-osc` is a **single** oscillator
   with one global `detune` value — no unison, no detune-spread, no 2nd/sub osc,
   no noise. (`src/shared/types/instrument.ts:46`; native `OscDef`
   `src/native/Instrument.h:35`; osc render `Instrument.mm:204`.) Listed only as
   *future* work in `docs/SOUND-DATA-MODEL.md:239-242`. → **The #1 ingredient of
   the sound is exactly the thing the engine can't do.**

2. **No true sustain from the grid.** Melodic voices auto-release ~one beat after
   trigger (`gateFrames = samplesPerBeat`, `Instrument.mm:261`); note `duration`
   is parsed by the renderer but **dropped** before the engine
   (`src/native/module.cpp:303-308`), and the 4-arg note-length callback refactor
   is half-wired (`src/native/Sequencer.h:24` vs `Sequencer.mm`/`AudioEngine.mm:347`).
   → A held drone note plays for a quarter-note then releases; you'd have to
   retrigger every step.

3. **`synth-osc` can't be auditioned or edited in the Sound Designer.** The Web
   Audio preview has **no oscillator path** — every voice starts from a sample;
   no sample → silence (`src/renderer/src/audio/previewEngine.ts:312`). The
   Sound Designer only authors `synth-sampler` (`soundDesignerStore.ts`). → You
   can't dial in an oscillator drone in-app even if you hand-wrote the JSON.

4. **Modulation is global-per-instrument, not per-voice.** One shared LFO phase
   drives all 16 voices (`Instrument.mm:282,304`) → stacked notes pump in
   lockstep, no organic drift. Slowest LFO via UI is 0.1 Hz (`ModTab.tsx:71`).

5. **Samples load mono** (`src/native/AudioEngine.mm:143 LoadWavMono`) → any
   stereo width baked into a source WAV is discarded on load. Width has to be
   re-created by the app's **chorus** + **stereo Freeverb** (`Reverb.mm`).

## What the app CAN do today (our leverage)
- **`synth-sampler` looping** with click-free crossfade at the loop seam
  (`Instrument.mm:184-200`) → a baked drone loop sustains cleanly forever while a
  note is held long enough (and long release + reverb bridge the per-beat gate).
- **Resonant SVF filter** — lp/hp/bp, 12/24 dB, Q≈0.5–10, pre-filter tanh drive
  (`Instrument.mm:233,320`; UI `FilterTab.tsx`). Enough for a Polivoks-ish tone
  on top of the baked loop.
- **Up to 4 LFO/env modulators** → filter cutoff (±4 oct), resonance, pitch
  (±12 st), pan (`Instrument.mm:297-302`; UI `ModTab.tsx`, min 0.1 Hz — slower by
  hand-editing JSON, no engine floor).
- **Insert FX** (2 slots): delay, reverb, drive, chorus (`src/shared/effects.ts`,
  native `Effect.mm`) + a global stereo **Freeverb** send (`Reverb.mm`).
- **16-voice polyphony per track**, equal-power gain staging (`Instrument.mm:418`).

## The bridge (what we're building)
```
                        DRONE LAB (standalone, outside the app)
  [ N voices × M detuned oscillators ] → [ Polivoks LP + drive ] → [ slow per-voice LFOs ]
        → [ noise/S&H ] → [ reverb/delay/chorus ] → live audition
        → Export: seamless MONO loop WAV  +  patch JSON
                                   │
                                   ▼
                   synth-sampler pack in the USER library
   samples[0] = the loop (root C2, loop:true)  +  app filter + slow LFOs + reverb
                                   │
                                   ▼
        Plays in a song today. Melodies played on a SEPARATE track sit on top.
```

## What this bridge gives up (documented trade-offs)
- **Baked, not live.** The oscillator movement is frozen into the loop; you can't
  re-tune detune per-note in the app. The app's filter/LFO/reverb still add live
  motion on top.
- **Mono source + pitch-shift.** One sample is pitch-shifted across the keys, so
  playing far from the root also speeds up the loop and shifts the beat rate.
  Mitigation: render at a **low root (C2)** and play within ~1 octave. (The Sound
  Designer authors only `samples[0]`, so no multisampling.)
- **Stereo width** must be re-added by the app (chorus + Freeverb), since the
  source is summed to mono.

## The real fix (later, not now)
See [`05-native-extension-proposal.md`](05-native-extension-proposal.md): add a
`unison`/oscillator-array to `synth-osc`, finish the sustained-note refactor, and
randomize per-voice LFO phase. Then a Solar 42 drone becomes a **pure JSON patch**
(parameter-first, per the house methodology) with no baked audio. The Drone Lab's
exported patch JSON is the validated blueprint for that work.
