# "Recipe for later" — native oscillator-stacking proposal (SPEC ONLY)

> **Status: proposal. Not implemented in this investigation.** This is the
> parameter-first path: with these three engine changes, a Solar 42 drone becomes
> a pure JSON `synth-osc` patch (no baked audio), consistent with the house
> methodology in `docs/CLASSIC_SOUNDS_INVESTIGATION.md`. The Drone Lab's exported
> patch JSON (`../patches/`) is the validated target these changes must reproduce.

All file references are to the repo at investigation time; confirm before editing.

## Change 1 — oscillator stacking / unison (the big one)
**Problem:** `synth-osc` has a single oscillator (`src/shared/types/instrument.ts:46`;
native `OscDef` `src/native/Instrument.h:35`), so no detune-beating — the core of
the sound.

**Option A (minimal): add a `unison` block to `OscillatorSpec`.**
```ts
// src/shared/types/instrument.ts
interface OscillatorSpec {
  waveform: 'sine'|'triangle'|'saw'|'square';
  octave: number; detune: number; level: number;
  unison?: {
    voices: number;        // 1..7
    detuneCents: number;   // total spread across the stack
    stereoSpread?: number; // 0..1 (needs a stereo voice path — see Change 4)
    blend?: number;        // center vs. side level
  };
}
```
- Native: in `OscSample` (`src/native/Instrument.mm:204`) run `voices` phase
  accumulators per note, spread over `±detuneCents/2` (uneven/microtonal spread
  reads more analog than symmetric), sum with `1/sqrt(voices)` gain.
- Preview: add an **oscillator source path** to `previewEngine.ts` (today it has
  none — `:312`); build `voices` `OscillatorNode`s with `detune` offsets. This
  also fixes "can't audition `synth-osc` in the Sound Designer".
- Watch band-limiting: naive saw/square already alias (`Instrument.mm:204`);
  stacking multiplies aliasing. Consider PolyBLEP or mild oversampling.

**Option B (fuller): make `oscillators` an array** (2–3 independent oscillators,
each with its own unison) — closer to Solar 42's per-oscillator tune/volume, more
work. Start with A.

## Change 2 — finish the sustained-note refactor (true drones)
**Problem:** held grid notes release after one beat; `duration` is dropped.
- `src/native/module.cpp:303-308` — stop discarding `duration`; pass it through.
- `src/native/Sequencer.h:24` already declares a 4-arg `NoteCallback` with
  `durationSteps`; wire the 3-arg call sites in `Sequencer.mm` /
  `AudioEngine.mm:347` to it, and set the voice gate from note length instead of
  the fixed `samplesPerBeat` (`Instrument.mm:261`).
- Add a real note-off so long notes sustain and release naturally.

## Change 3 — per-voice modulation drift
**Problem:** one shared LFO phase for all voices (`Instrument.mm:282,304`) →
lockstep. Give each active voice its own LFO phase seeded at note-on (small random
offset). Expose sub-0.1 Hz rates in the UI (`ModTab.tsx:71`; the engine has no
lower floor already).

## Change 4 (optional) — stereo voice path
Samples/voices are mono until the pan stage. For per-oscillator stereo spread
(Change 1 `stereoSpread`) you'd carry L/R through the voice sum. Lower priority —
the master Freeverb + chorus already provide usable width.

## Suggested target patch (what the engine must reproduce)
The Drone Lab exports this shape into `../patches/`. Rough form:
```jsonc
{
  "type": "synth-osc",
  "oscillator": { "waveform": "saw", "octave": 0, "detune": 0, "level": 1,
                  "unison": { "voices": 6, "detuneCents": 22, "stereoSpread": 0.8 } },
  "envelope": { "attack": 1.2, "hold": 0, "decay": 0, "sustain": 1, "release": 4.0 },
  "filters": [{ "type": "lowpass12", "cutoff": 900, "resonance": 0.55, "drive": 0.35 }],
  "modulators": [
    { "wave": "triangle", "rate": 0.05, "amount": 0.25, "destination": "filter-cutoff" },
    { "wave": "sine",     "rate": 0.08, "amount": 0.05, "destination": "pitch" }
  ],
  "effects": [ { "type": "reverb", "params": { "size": 0.7, "mix": 0.4 } },
               { "type": "chorus", "params": { "rate": 0.3, "depth": 0.5, "mix": 0.5 } } ]
}
```
When Changes 1–3 land, load this, hold a chord, and A/B against the baked-sample
version + SOUND DEMO 3. Parity = done.

## Sequencing / effort estimate
1. Change 2 (sustain) — smallest, unblocks all pads/drones, standalone win.
2. Change 1 Option A (unison) + preview osc path — the core; medium effort.
3. Change 3 (per-voice drift) — small polish once 1 is in.
4. Change 4 (stereo voices) — optional, later.
