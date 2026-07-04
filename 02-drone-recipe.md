# Solar 42 drone — the recipe (in our parameter vocabulary)

A parameter-first restatement of the Solar 42 sound, using the vocabulary of our
own engine + the Drone Lab. Numbers are **starting points to tune by ear**, not
gospel. Confidence: **[H]/[M]/[E]** as in `01-architecture.md`.

## One "drone voice" = a unison oscillator stack
The core building block. One held pitch, thickened into a beating cluster.

```
voice = {
  rootNote:      C2..C3          # low; leaves headroom to play melodies above  [E]
  oscillators:   4..7 per voice  # Solar 42 ~5; 5 is the sweet spot             [H]
  waveform:      saw | triangle  # saw = rich/bright, triangle = soft/hollow    [M]
  detuneSpread:  ±6..±18 cents   # cents between adjacent oscillators           [H]
                                 # small = slow beats; wide = shimmery/chorusy
  detuneShape:   uneven          # microtonal, NOT symmetric equal steps        [M]
  stereoSpread:  wide            # pan each osc across L/R (audition only; app
                                 # sums to mono — width comes back via app FX)   [H]
  level:         equal-power     # 1/sqrt(N) so the stack doesn't clip          [H]
}
```

**Why it works:** two saws 10 cents apart beat at ~(f·0.006) Hz — sub-Hz at low
pitches. Five oscillators at uneven spreads → a dense set of slow beats = the
"breathing" with no LFO at all. This is the single most important parameter.

## Layer a few voices (for the 6-block density)
Stack **2–4** of the above at different pitches to build a chord/cluster:
- Root, +7 (fifth), +12 (octave) is a safe cinematic bed. **[E]**
- For the eerie Solar 42 flavour, add one voice a **microtonal** interval off
  (e.g. +3.5 semitones, or the fifth detuned ~15 cents). **[M]**
- Optionally one very low sub voice (root −12) at low level for weight. **[E]**

## Noise / grit (the Papa-Srapa + S&H texture) — secondary
```
noise = { level: 0..15%, tone: dark (LP ~2 kHz), movement: slow S&H on level/tone }
```
Keep it low; it's seasoning, not the main dish. **[M]**

## Filter — Polivoks character
```
filter = {
  type:       lowpass 12 dB (start) → try 24 dB for a darker bed
  cutoff:     400..1500 Hz          # low enough to be a "bed", opens for melodies [E]
  resonance:  0.4..0.7 (of 1.0)     # high-ish for the "singing" edge; back off if it whistles [H]
  drive:      0.2..0.5              # pre-filter tanh → warmth + harmonics       [H]
}
```
Push resonance until you hear a vocal formant riding the drone, then pull back
~10%. That formant is a signature Polivoks trait. **[H]**

## Movement (on top of the intrinsic beating) — light touch
```
lfo1 = { shape: triangle, rate: 0.03..0.1 Hz, dest: filter cutoff, depth: small }
lfo2 = { shape: sine,     rate: 0.05..0.15 Hz, dest: pitch, depth: ±3..8 cents }
perVoicePhase: randomized   # each voice's LFOs start at a different phase → drift
```
Rates are **glacial** (10–60 s cycles). Per-voice phase offsets are what make the
voices drift independently instead of pumping in lockstep. **[M]**

## Space
```
reverb = { size: large, decay: 3..6 s, mix: 30..50% }
delay  = { time: 300..500 ms, feedback: 0.3..0.45, mix: 15..25% }  # optional
chorus = { rate: 0.2..0.5 Hz, depth: moderate }                    # adds width post-mono
```
Lush and long. In the app, chorus + the stereo Freeverb are what restore stereo
width after the mono sample load. **[H]**

## "How do I know it's right?" — listening checklist
- [ ] It **moves** while holding one chord, with **no** obvious single-LFO pulse.
- [ ] There's a resonant/vocal edge that opens as you raise cutoff.
- [ ] It's **wide** and reverberant, not a dry mono buzz.
- [ ] A melody played on a separate lead sits **above** it without mud.
- [ ] A/B against SOUND DEMO 3: same *class* of churning cinematic drone.

## Two secondary Polivoks presets to derive later (bass/lead)
The same stack + hotter drive + lower cutoff = **Polivoks growl bass**; narrower
stack + high resonance + envelope on cutoff = **Polivoks screaming lead**. These
map onto Classics targets #9–10 and reuse this recipe. **[E]**
