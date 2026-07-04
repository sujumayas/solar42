# Solar 42 — official manual digest (verified reference)

> Source: **Elta Music official user manuals**, downloaded 2026-07-03 into
> [`reference-docs/`](reference-docs/). Everything here is from the maker's own
> documentation (confidence [H] throughout), unlike `01-architecture.md` which
> was assembled from press. **Where the two disagree, this file wins** —
> corrections are flagged inline with ⚠️.
>
> - `reference-docs/solar42-user-manual-elta-official.pdf` — Solar 42, 28 pp (`solar42_instruct_01_24`)
> - `reference-docs/solar42f-manual-elta-official.pdf` — Solar 42F revision, 28 pp
> - `reference-docs/solar42n-manual-elta-official.pdf` — Solar 42N revision, 28 pp
> - `reference-docs/solar50-owners-manual-elta-official.pdf` — predecessor (drone-voice lineage)
> - `reference-docs/elta-cartridge-diy-kit.pdf` — **effector platform revealed: Spin FV-1**
> - `reference-docs/elta-cartridges-catalog.pdf`, `elta-cartridge-ochre-reference-card.pdf` — FX cartridge param maps
> - `reference-docs/solar42-manual-juno.pdf` — 2-page keyboard quick reference
> - `reference-docs/solar42-block-scheme-p6.png` — block diagram (rendered from manual p. 6)
> - Extracted text: `solar42-manual.txt`, `solar42f-manual.txt`, `solar42n-manual.txt`
>   (searchable, page numbers refer to the PDFs)

## Top-level architecture (manual p. 6, block scheme)

**8 voices → 10-channel stereo mixer (vol + pan per channel) → dual Polivoks
VCF (L + R) → dual FV-1 effector (L + R) → WET OUT L/R.** Voices 4/5 also have
pre-filter DRY OUTs. Extra mixer channels: EXT IN and the piezo PREAMP.

**Pan position routes each voice to the filters**: hard left → Filter L only,
hard right → Filter R only, centre → both. Panning *is* the filter-routing
matrix — a distinctive feature worth keeping in a recreation.

CV sources: 2 panel LFOs, joystick (X/Y), 5-step sequencer (CV/gate/clock),
per-voice envelope outs, touch-keyboard (V/oct, 2 gates, pressure CV, clock),
drone keypad gates, envelope follower (CV + gate).

## The 8 voices — three different circuits

⚠️ Correction to `01-architecture.md`: it is **not** "6 drone blocks × ~5 osc".
The manual defines three voice types:

| Voices | Type | Sound engine |
|---|---|---|
| 1, 2, 6, 7 | "Classic" drone (Solar 50 lineage) | **5 free-running sawtooth generators** each |
| 3, 8 | "New" drone (Papa Srapa circuit) | **2 Schmitt-trigger oscillators** + white noise |
| 4, 5 | VCO voices | **AS3340 triangle-core VCO** + ADSR/VCA |

### Classic drone voices 1, 2, 6, 7 (manual p. 7)

- 5 simple **sawtooth** generators per voice. **No V/oct** — each has only a
  TUNE knob. Staggered factory ranges (approximate, unit-to-unit variance):
  - gen 1: **C0–G2** · gen 2: **B1–G3** · gen 3: **D3–B4** · gen 4: **E4–C6** · gen 5: **G5–D7**
  - → one voice spans ~8 octaves (B0–D7). This is a **chord/cluster machine**,
    not a unison stack: you tune intervals (or near-unisons for beating) by ear.
- Per-generator **MUTE** button (build dyads/triads/full clusters).
  ⚠️ No per-generator volume — level is per-voice, in the mixer.
- Per-generator **MOD** button: routes pitch modulation from **either** the CV
  MOD input jack **or the built-in photo-sensitive detector** (red-LED
  optocoupler; ambient light leaks in — daylight-bulb flicker is audible).
  With MOD off the generators run **stable**, no drift.
- **VOLT knob**: transposes all 5 generators **down** together; past ~half
  travel the generators start **cross-modulating (FM)** — the dirty zone.
- **AR envelope (ATT/RLS) + VCA** per voice. Gate comes from the drone keypad
  (or external CV). **HOLD** latches the gate → endless drone. `env out` jack
  can chain-trigger the next voice or sweep filter/effector.

### Papa Srapa voices 3, 8 (manual p. 8)

Based on Papa Srapa's DIY noise synths. Two **Schmitt-trigger oscillators**:
one sub-audio (RATE knob + x1/x10 switch → square modulator, also on `cv out`),
one audio-rate **C0–E7** (PITCH knob + RANGE switch). FM and AM toggles combine
them into 5 documented modes:

1. FM+AM off → plain continuous drone (PITCH/RANGE set tone).
2. FM on → square-wave pitch wobble; depth via FM AMOUNT + DIVIDER.
3. AM on → intermittent/chopped tone at RATE.
4. FM+AM on → chopped *and* pitch-modulated (sirens, birds, gunfire volleys).
5. All knobs left, switches down, NOISE up → **pure white noise**.

The voice's white noise is normalled to its **S&H generator** (`in` jack);
patch any LFO/clock into `clock` to get stepped random (S&H out ±5 V). Same
AR + VCA + HOLD gate scheme as the classic voices.

### VCO voices 4, 5 (manual p. 9)

- **AS3340** triangle-core analog VCO. **6 waveforms**, including two
  **morphing** ones: saw→inverted-saw and sine→triangle (wave selector).
- SHAPE (pulse width) with CV input + attenuator; **linear FM** input with
  attenuator; 8-position octave switch (**64' 32' 16' 8' 4' 2' 1' + LO**);
  TUNE spans one octave; **V/oct input**.
- Voice 4 has a `vco out` jack; voice 5 has a `sync` input, and **VCO 4's out
  is normalled to VCO 5's FM input** (instant 2-op FM via the FM attenuator).
- Each has a full **EG + VCA: A/D/S/R plus LOOP mode and HOLD** (VCA always
  open). `vca cv` input for external amplitude control.
- Touch keyboard V/oct + gate are normalled to **both** voices 4 and 5.

## Modulation sources

- **LFO A (slow) and LFO B (fast)** — WAVE knob **morphs square↔triangle**
  (centre = mix). **Unipolar 0→+10 V** by design, to drive the drone voices'
  photo-sensor LEDs. ("Five LFO generators" in the spec counts these two plus
  the pulser and the two Papa Srapa rate oscillators.)
- **Joystick** — game-console style, position-locking; X/Y each with an offset
  knob; output **−10…+10 V**, 0 V centred.
- **5-step sequencer** (Buchla-inspired) — per-step CV knob **0–5 V**, per-step
  gate on/off switches (gates mutable without affecting CV), STAGES switch
  (3/4/5 steps), internal **PULSER** clock (clock out jack) or EXT clock in.
- **Envelope follower** on the preamp — attack/release, env CV 0…+10 V,
  gate out 0…+8 V.
- **Per-voice envelope outs** — drone voices ±10 V.

## Touch keyboard (manual pp. 13–20)

12 capacitive plates + 2 transpose buttons + encoder. Semi-digital (MCU + DAC).

- **V/oct out 0–8 V** (8 octaves), normalled to VCOs 4/5; GATE L/R 0–10 V;
  PRESSURE out (see below); CLOCK in; RESET in (arp reset).
- **Behaviours**: single (12 keys) / **twin** (2×6 keys, pressure out becomes
  right side's V/oct) / **split** (2 sides, separate params).
- **Modes**: keyboard / arpeggiator / **16-step sequencer** per side.
- **Quantiser**: per-note scale editor + preset scales (Semitones, Ionian,
  Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Blues maj/min,
  Pentatonic maj/min, Folk, Japanese, Gamelan, Gypsy, Arabian, Flamenco,
  Whole tone) + root note. ⚠️ **Quantiser off = fully microtonal keyboard**
  (plates hand-tunable in 0.0025 V steps) — `01-architecture.md`'s "no
  quantizer" was wrong; it's optional.
- **Arpeggiator**: hold, clock mult/div, direction (fwd/back/ping-pong/random),
  variation ×1–3 transposed by a set interval (1–12 semitones), and a 1–8 step
  **rhythm gate mask** on the clock.
- **Sequencer**: 2–16 steps, free or key-advanced, directions, per-step
  gate/value editor, continuous-vs-gated CV update, rhythm mask.
- **Portamento** (0–255, optional legato-only), **vibrato** LFO
  (speed/depth/delay, optionally pressure-controlled).
- **Pressure output modes**: raw pressure (capacitive area, not force) / ASR /
  AD / looped AD / random-per-touch, with RISE/FALL slew (0–255).
- Internal clock 10–300 BPM; auto-switches to external clock. 4 presets.

## Mixer, filters, effector (manual pp. 21–22)

- **Mixer**: VOL + PAN for all 8 voices, EXT AUDIO, and PREAMP (10 stereo
  channels). Pan = filter routing (see top).
- **Dual Polivoks filter**: classic Soviet topology using **UD12
  (УД1208) op-amp chips**, 12 dB/oct **low-pass**. FREQ, RES, CV-in (left
  filter), **LINK** switch slaves both cutoffs to the one CV. Chip tolerances
  make L and R filters **audibly slightly different — intentional stereo
  "live" width**. Resonance does **not** thin the bass. Character: "dirty and
  angry".
- **Dual effector**: same circuit as the Elta **Console pedal / Solar 50** —
  confirmed in `elta-cartridge-diy-kit.pdf` to be a **Spin FV-1 DSP** per
  channel. Cartridges are just **24LC32A EEPROMs** holding 3 programs (FV-1
  internal slots 5, 3, 7). One cartridge program loads per channel (L/R can
  differ; program persists in RAM after cartridge removal until reload).
  **X, Y, Z knobs/CV (−10…+10 V), BLEND (dry/wet) and MASTER are shared by
  both channels.** Two slightly-different FV-1s L/R → same "live stereo"
  philosophy as the filters.
- Output: WET OUT L/R max ~2 V; DRY outs (V4/V5) max ~1 V.

### FX cartridge catalog (manual pp. 23–24; X/Y/Z per program)

| Cartridge | Theme | P1 | P2 | P3 |
|---|---|---|---|---|
| CATHEDRAL | Reverbs | Shimmer (X oct-up, Y oct-down, Z decay) | Oct-up delay (X fb, Y delay, Z reverb) | Space reverb (X fb, Y delay, Z reverb) |
| TIME | Mod delays | Delay reverb (X fb, Y delay, Z reverb) | Delay chorus (X fb, Y delay, Z mod depth) | Delay vibrato (X fb, Y delay/rate, Z mod depth) |
| MAGIC | Pitched delays | Pitch delay (X fb, Y delay, Z pitch) | Reverse pitch delay (X fb, Y delay, Z pitch) | Bell pitch delay (X fb, Y delay, Z pitch) |
| VIBROTREM | Modulation | Tremolo (X depth, Y rate, Z reverb) | Vibrato (X depth, Y rate, Z reverb) | Chorus (X depth, Y rate, Z reverb) |
| FILTER | Filter/wah | Auto-wah (X amt, Y env, Z reverb) | HP/LP filter (X HP, Y LP, Z res) | Notch filter (X cut1, Y cut2, Z res) |
| VIBE | Rotary/phase | Phaser (X depth, Y rate, Z reverb) | Flanger (X depth, Y rate, Z reverb) | Resonance flanger (X res, Y rate, Z mod depth) |
| PITCH SHIFTER | Octaves | SynthTaver (X oct-dn, Y oct-up, Z direct) | Octaver (X oct-dn, Y oct-up, Z direct) | Pitch harmonizer (X p1, Y p2, Z mix) |
| INFINITY | Big ambient | Resonance reverb (X pre-dly, Y pre-dly mod, Z decay) | O.D.D. oscillating dirty delay (X fb, Y delay, Z pitch) | Resonance delay (X fb, Y delay, Z pitch) |
| STRING RINGER | Audio-rate mod | Synthetic ring (X freq, Y res, Z sub) | Ring mod (X freq, Y rate, Z reverb) | S&H ring mod (X pitch spd, Y S&H rate, Z ring freq) |
| SYNTEX-1 | Bass synth | Vibe synth (X vib rate, Y res, Z sub) | Pulse synth (X trem rate, Y res, Z sub) | Acid synth (X tone, Y color, Z sub) |
| DIGITAL | Bit/SR crush | Filter DAC (X SR, Y cutoff, Z gain) | LFO DAC (X SR, Y LFO spd, Z LFO amt) | Envelope crusher (X SR, Y env amt, Z gain) |
| GENERATOR | Noise mini-synth | FM tone (X p1, Y p2, Z FM) | Ramp (X LFO rate, Y pitch, Z pitch mod ±) | Voice (X cutoff, Y pitch, Z LP/HP) |
| OCHRE | Reverse delays | One-shot long (X time, Y fb, Z trig threshold) | One-shot short (same XYZ) | Free-run loop (X time, Y fb, Z delay-mod LFO/RND) |

## Voltage/spec quick table (manual p. 4)

| Signal | Range |
|---|---|
| VCO out | ±5 V · DRY V4/V5 max 1 V · WET max 2 V |
| EG out | 0–8 V · drone-voice env outs ±10 V |
| LFOs | 0…+10 V (unipolar) · Pulser ±10 V |
| S&H | ±5 V · Voice 3/5 modulator 0…+12 V |
| Joystick | ±10 V (offsettable) |
| 5-step seq | CV 0–5 V, gate 0–10 V |
| Env follower | CV 0–10 V, gate 0–8 V |
| Keyboard | V/oct 0–8 V, gates 0–10 V |

Fully **analogue** except the touch keyboard and effector (semi-digital).
Power DC 12 V 1–2 A; 49.5 × 32 × 2.9 cm; 5.4 kg.

## 42F / 42N revision deltas (from their manuals)

- **42F**: filter gains **LP/BP mode switch** + **"double distortion"** stage
  after the filter; VCOs gain a **sub-generator (−1 oct) switch** and
  **lin/exp CV mode switch**; octave switch becomes +3/LOW style; joystick LED;
  toggles → push buttons; V4/V5 dry outs moved to front; lower noise floor.
- **42N**: adds **LFO speed range switches** and **CV input on the right
  filter** (previously only left + LINK).
- Voice complement (4 classic + 2 Papa Srapa + 2 VCO) is unchanged across
  42 / 42F / 42N.

## What this changes for our recreation

1. **Drone Lab's "detuned unison stack" is only half the story.** A classic
   Solar drone voice is 5 *independently tuned* saws across staggered ranges —
   interval clusters (root/fifth/octaves slightly off) with per-osc mute, not N
   copies of one pitch. Drone Lab should grow a **per-oscillator coarse tune**
   (or interval preset: unison-beat / fifth / octave cluster) on top of the
   cents detune.
2. **Stability is a feature**: with MOD off the hardware oscillators do *not*
   drift. The movement is beating + optional photo-sensor/LFO mod. Our
   per-voice "pitch drift" should default to ~0 and be re-badged as the MOD
   path (slow chaotic unipolar mod ≈ ambient-light sensor).
3. **The FX are FV-1 algorithms** — publicly well-understood (SpinCAD block
   library, spinsemi.com docs). Our delay/chorus/reverb approximations are the
   right family; a "Shimmer" (oct-up reverb) and a reverse delay would cover
   the most Solar-typical textures. FV-1 = 32 kHz sample rate, ~1 s delay
   memory — its lo-fi darkness is part of the sound.
4. **Stereo realism trick**: run L and R filter (and FX) with slightly
   different parameters (±few %) to mimic component tolerance — cheap and very
   effective.
5. **Pan-routes-to-filter** is a characterful routing worth copying (voice pan
   position crossfades its send between Filter L and Filter R).
6. **Papa Srapa voice** ≈ square osc + square sub-audio modulator with
   FM/AM switches + noise → S&H. Trivially reachable in Web Audio; adds the
   siren/space-monster palette Drone Lab currently lacks.
7. **VCO pair recipe**: 2 morphing-wave VCOs, VCO4→VCO5 linear FM normal,
   shared keyboard CV, ADSR with loop — this is the melodic layer over the
   drone bed (matches our keyboard layer in Drone Lab).
