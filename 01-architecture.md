# Solar 42 — architecture & what makes the sound

> Sourced from Elta Music's own spec page + press coverage (see
> [`sources.md`](sources.md)). Confidence tags: **[H]** high (multiple sources
> agree), **[M]** medium (single source / press paraphrase), **[E]** my estimate.
>
> **⚠️ 2026-07-03: superseded by [`06-manual-digest.md`](06-manual-digest.md)**,
> which is built from the official user manuals (downloaded to
> `reference-docs/`). Where this file and the digest disagree — notably the
> drone-voice breakdown (it's 4 classic × 5 saws + 2 Papa Srapa, not 6 × 5) and
> the keyboard quantiser (it exists, it's optional) — trust the digest.

## What it is
The **Elta Music SOLAR 42** is an **analogue microtonal ambient drone machine** —
a boutique hardware instrument built for film/theatre soundtracks, atmospheres,
and live ambient performance. Design lineage nods to Theremin-era electronics and
1950s sci-fi soundtracks. It is the successor to Elta's SOLAR 50. **[H]**

It is **not** an app or plugin. Our job is to recreate its *sound*, not the box.
Knob settings and synthesis techniques are not copyrightable, so recreating the
recipe in our engine is safe. **[H]**

## Signal architecture (as best documented)

| Block | Detail | Conf |
|---|---|---|
| **Drone voices** | **6 drone blocks**, each with **~5 oscillators**, every oscillator having its own **tune** and **volume** → up to ~30 oscillators stacked. Triggered/held via 6 touch-pads → "endless oscillation". | [H] osc-per-block count [M] |
| **Melodic voices** | **2 VCO voices** with **morphing waveforms** and **1V/Oct** pitch in (voices 4 & 5). Playable via keypad / sequencer / external CV. Can also run as LFOs. | [H] |
| **Noise / chaos** | **2 white-noise generators**; two of the drone voices (the "blue" ones, 3 & 6) are based on Papa-Srapa DIY noise-synth circuits → grittier/less stable. | [H]/[M] |
| **Random** | **2 Sample-&-Hold** generators (stepped random). | [H] |
| **Modulation** | **5 LFOs**, a **5-step sequencer**, and a **joystick**. | [H] |
| **Filter** | **Dual analogue 12 dB Polivoks-inspired filter** (Soviet character: aggressive, resonant, can get "singing"/unstable). One per output channel. | [H] |
| **Effects** | **Dual cartridge effector-combiner** with CV control — swappable delay/reverb/etc. per channel. | [H] |
| **Stereo** | All-new **stereo signal path**; oscillators pan **independently** L/R. | [H] |
| **Tuning** | **Microtonal** — oscillators tuned freely by ear, no forced equal-temperament / no quantizer. | [H] |
| **I/O** | Stereo out; contact-mic preamp **input with envelope follower**; CV -10V..+12V. | [H] |

## The four things that actually make "the sound"
Distilled — this is what we must reproduce:

1. **Stacked detuned oscillators = the movement.** The slow, breathing evolution
   of a Solar 42 drone is **beating** between many oscillators tuned a few cents
   apart, *not* a single LFO sweep. With ~5 oscillators per voice and 6 voices,
   you get a dense field of interference beats at many rates → the sound is never
   static even with zero modulation. **This is the #1 ingredient.** **[H]**
2. **Microtonal / non-equal detune** widens and enriches the beating vs. tidy
   equal-tempered stacks — the reason it sounds "organic" and slightly eerie. **[M]**
3. **Polivoks-flavoured resonant filter + drive** gives the warm-but-edgy timbre;
   pushed resonance adds a vocal/"singing" formant on top of the drone. **[H]**
4. **Wide stereo + lush reverb/delay** turns a mono oscillator bank into a
   cinematic wash. Independent per-oscillator panning is a big part of the width. **[H]**

Supporting texture, secondary: **noise + S&H** shimmer/grit, and slow LFOs for
extra filter/pitch drift on top of the intrinsic beating.

## The reference clip (SOUND DEMO 3)
`https://youtu.be/dzU8T2lKUhs` — Elta's official demo. Character to match: a
sustained, harmonically rich pad-drone that slowly churns, with clear stereo
width and a resonant/vocal edge, plus melodic notes floating above the bed. Our
success target is *this class of sound*, auditioned by ear (see `00-LOG.md`).

## Precedent: it has been emulated in software
A VCV Rack "ELTA Solar 42 Emulator" exists (community.vcvrack.com). Notes from
that thread: it rebuilds all 8 voices + stereo mixer + sequencer + LFOs +
filters; tuned **by ear** (no quantizers), some voices use linear FM as an
approximation. Confirms the sound is reachable with ordinary
oscillator+filter+FX building blocks — exactly what our Drone Lab provides. **[M]**
