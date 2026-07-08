# Solar 42N — complete panel & behavior inventory (build reference)

> Compiled 2026-07-03 from the official manuals (`reference-docs/solar42-manual.txt`,
> `solar42f-manual.txt`, `solar42n-manual.txt`) and the 42N panel render
> (`reference-docs/solar42n-panel-1.png` / `solar42n-render-spec.pdf` — same art,
> PDF is higher-res). This is the **implementation source of truth for the digital
> 42N**: it resolves the voice renumbering vs [`06-manual-digest.md`](06-manual-digest.md)
> (which uses base-42 numbering), lists every control/jack, all normalled
> connections, the voltage table, and — critically — everything the manuals do
> **not** specify (→ `app/src/dsp/TuningConstants.h`).

## 0. Revision & numbering (read this first)

We recreate the **Solar 42N**. The panel render is 42N; the deepest manual is the
base 42. The revisions renumber the voices:

| Concept | base 42 | **42N (ours)** |
|---|---|---|
| Classic 5-saw drone voices ×4 | voices 1, 2, 6, 7 | **DRONE 1, 2, 4, 5** |
| Papa Srapa noise voices ×2 | voices 3, 8 | **DRONE 3, 6** |
| V/oct AS3340 voices ×2 | voices 4, 5 | **VCO A, VCO B** |
| Drone keypad button grid | 1,6 / 2,7 / 3,8 | **1,4 / 2,5 / 3,6** (2 cols × 3 rows) |
| VCO sync/out jacks | V4 = vco out, V5 = sync in | **VCO A = SYNC in, VCO B = OSC out** (swapped!) |

⚠️ The LFO chapter in *all three* manuals says photosensors are on "voices
1, 2, 6, 7" — copy-paste residue from base numbering. On the 42N the photosensor
(classic) voices are DRONE 1, 2, 4, 5.

**42N deltas vs base 42** (42F p26 + 42N p26):
- Filter: **BP/LP mode switch** per channel; post-filter **"double distortion"**
  (DIST = clean/dirty crossfade, GAIN = distortion amount, shared L/R); new
  stable circuit (no UD12 chips, no tuning drift); **CV R jack** added, **CV L
  normalled to CV R**; **MOD L / MOD R** knobs = per-channel filter CV amounts.
- LFO A/B: **x1/x6/x10 speed range switch**.
- VCOs: **−1 sub-oscillator switch** (square, one octave down), **lin/exp CV
  response switch**, octave = **oct+3 / low** switches (replaces 8-pos rotary).
- Effector: **simplified program loading** — flipping a channel's 1‑2‑3 toggle
  loads that program (base/42F needed a load-button procedure).
- Joystick gets position LEDs; toggles → push buttons; VCO dry outs on front
  panel; knob & jack color coding (below); lower noise floor.
- Classic drone generators described as **negistor** (single-transistor) circuits.

**Panel color code** (42N p26) — replicate in UI:
knobs: **black** = classic-drone TUNE + mixer PAN · **blue** = VOLT + all Papa
Srapa knobs · **green** = VCO + Envelope A/B · **grey** = mixer VOL · **orange**
= filter + effector + master · **red** = LFOs, joystick, sequencer, preamp,
env follower. Jack labels: **red = output, black = input**.

## 1. Panel layout (42N render; panel 49.5 × 32 cm)

Overall: upper two rows of modules; center column = effector → filter → mixer;
bottom red modulation strip; below that the performance zone (physical joystick
left, 12-plate touch keyboard center, drone keypad right).

```
[EXT.AUDIO  WET OUT R/L]   SOLAR 42N ... AMBIENT MACHINE ...   [POWER 12V DC]
[DRONE 1][DRONE 2] [ DUAL EFFECTOR: slot, cvX/Y/Z, X Y Z, BLEND, MASTER, phones ] [DRONE 4][DRONE 5]
                   [ FILTER L: FREQ RES BP/LP | CVL DIST link GAIN CVR MODL MODR | FILTER R: FREQ RES BP/LP ]
[DRONE 3][ VCO A ] [ VOICE MIXER: 10× (PAN over VOL) + ENV A | logo | ENV B    ] [ VCO B ][DRONE 6]
[LFO A][JOYSTICK][5 STEP SEQ. VOLTAGE][PREAMP][ENVELOPE FOLLOWER][LFO B]
[joystick]        [==== 12-plate touch keyboard, encoder+display ====]   [DRONE VOICES keypad]
```

Mixer channel order (left→right): **DRONE 1 · DRONE 2 · DRONE 3 · EXT.AUDIO ·
VCO A · VCO B · PREAMP · DRONE 4 · DRONE 5 · DRONE 6**.

### 1a. Classic drone voice ×4 (DRONE 1, 2, 4, 5) — manual p7

- 5 generator columns × 3 rows labelled **MUTE / TUNE / MOD** top-to-bottom:
  per generator a MUTE push-button (top, column numbers 1–5 printed above),
  TUNE knob (black), MOD push-button (bottom, above the GATE/HOLD block).
  (Corrected 2026-07-07 against the render-spec art + manual print — an
  earlier revision of this doc had MOD/MUTE swapped.)
- **Red 5-LED bar** "1 2 3 4 5" = generator activity.
- **VOLT** knob (blue): transpose-all-down; past ~half = cross-FM dirty zone.
- **Photo-sensor**: white circular window (LDR + red LED opto).
- Bottom row: **GATE** in jack · **HOLD** button (yellow LED) · **ATT** knob ·
  **RLS** knob · **CV** in jack (pitch-mod bus for MOD buttons) · **env out**
  jack (red). Voice number badge in corner.

### 1b. Papa Srapa voice ×2 (DRONE 3, 6) — manual p8

- Header "LFO … 1⋯10 … fm→am" (red routing arrows), "DRONE 3"/"DRONE 6".
- Blue knobs: **rate** (sub-audio Schmitt osc) · **mod** (FM amount) ·
  **divider** · **PITCH** (audio Schmitt osc) · **NOISE**.
- Switches: **fm** on/off · **am** on/off · **x1/x10** (rate mult) ·
  **hi/low** (pitch range).
- Jacks (7 — recounted M9b P4 vs manual p8 + render spec): **cv out** (the
  square modulator, 0…+12 V; print marks it red with no text, under the
  rate/mod gap) · **cv in** (pitch CV into the audio osc, "▲cv" under the
  mod/divider gap; law unspecified by the manual) · **GATE** in ·
  **env out** · **S&H box:** in (normalled from own white noise) +
  clock in + out.
- **GATE/HOLD/ATT/RLS** row as classic voices. No photosensor, no VOLT.

### 1c. VCO voice ×2 (VCO A, VCO B) — manual p9

- Green knobs: **cv amt** · **tune** (1 octave) · big central **MORPHING
  WAVEFORM** rotary (6 positions, 2 morph zones) · **pwm** · **pw**.
- Switches: **oct+3** / **low** (octave) · **−1 sub** · **lin/exp**.
- Jacks: **1v/oct** in · **cv** in · **pwm** in · **sync** in (VCO A) /
  **osc** out (VCO B, red label).
- **Envelope A / Envelope B** (center panel, flanking mixer logo): green
  **A D S R** knobs, **hold** button, **loop** (self-generation) mode; jacks
  **gate** in · **env** out · **vca cv** in.

### 1d. Filter + effector (center) — manual pp21–22

- **FILTER L / R**: FREQ, RES (orange), BP/LP switch each. Shared center:
  **CV L** in · **DIST** · **link** (latching **push button** — manual "when
  switched on"; flat black circle on the print, M9b P4) · **GAIN** ·
  **CV R** in · **MOD L / MOD R** (CV amount per channel).
- **DUAL EFFECTOR**: cartridge slot; **cv x / cv y / cv z** in jacks; **X, Y, Z**
  knobs; two **1‑2‑3** program toggles (one per channel — flipping loads that
  program from the inserted cartridge); **BLEND** (wet level, shared); **MASTER
  VOLUME**; headphone jack + level. No output VCA — manual warns not to max
  MASTER (recommend ≤ 3 o'clock).

### 1e. Bottom modulation strip — manual pp10–12

| Block | Knobs | Switches | Jacks | Notes |
|---|---|---|---|---|
| LFO A (slow) | wave (sq↔tri morph), rate | x1/x6/x10 | out | unipolar 0…+10 V (drives photo-LEDs) |
| JOYSTICK | X offset, Y offset | — | X out, Y out | ±10 V; position-locking stick |
| 5 STEP SEQ | pulser + step 1–5 | stages 3/4/5, 5× gate toggles | ext clock in, clock out, cv out, gate out | CV 0–5 V, gate 0–10 V, pulser ±10 V |
| PREAMP | gain (≤40 dB) | — | ext. source in (1 MΩ) | internal piezo normalled; clip LED |
| ENV FOLLOWER | attack, release | — | env out, gate out | 0–10 V / 0–8 V |
| LFO B (fast) | wave, rate | x1/x6/x10 | out | unipolar 0…+10 V |

### 1f. Touch keyboard + keypad — manual pp13–20

- 12 capacitive plates (pressure = **contact area**, not force), 2 transpose
  buttons, push-encoder + small display, status icons/LEDs, sun logo.
- Jacks: **V/OCT** out 0–8 V · **GATE L** out · **GATE R** out (0–10 V) ·
  **PRESSURE** out 0–8 V · **CLOCK** in · **RESET** in (0…+5 V rec.).
- **DRONE VOICES** keypad: 6 buttons, columns **1,4 / 2,5 / 3,6** — momentary
  gates for the drone voices (HOLD latches).

### 1g. Master I/O

EXT. AUDIO in (mixer channel) · WET OUT L/R (max ~2 V) · DRY OUT for VCO A / B
(max ~1 V; position resolved during M5: the red "VCO A"/"VCO B" jacks in the
envelope A/B jack rows, flanking the center logo) · headphones · POWER +
12 V DC.

Registry census corrections (M5, append-only — existing jack ids frozen):
`vcoA.dry.out` / `vcoB.dry.out` (the DRY OUTs above, osc x 0.2) and
`pre.ext.in` (the preamp's ext. source jack, §1e — overrides the host-input
"piezo" when patched).

## 2. Normalled connections (default patch — break when a cable is inserted)

1. Touch keyboard **V/OCT → VCO A and VCO B `1v/oct`**.
2. Touch keyboard **GATE L → Envelope A and Envelope B `gate`** (GATE R takes
   the right side in twin/split behaviours).
3. **VCO A out → VCO B `cv`** (through VCO B's CV AMT attenuator) = instant
   2-op FM.
4. Each Papa Srapa voice's **white noise → its own S&H input** (S&H advances
   only when a clock is patched into its `clock` jack).
5. **Internal pulser → 5-step sequencer clock** (ext `clock` in overrides).
6. **Filter CV L → CV R** (42N); **link** switch slaves both cutoffs to CV L.
7. **Internal piezo mic → preamp** (ext. source jack overrides).
8. **Ambient light → each classic voice's photo-sensor**; the sensor (or the
   voice's CV jack, if plugged) feeds any generator whose MOD button is on.

## 3. Voltage / range table (manual p4, all revisions)

| Signal | Range |
|---|---|
| VCO audio out | ±5 V (10 Vpp) |
| DRY OUT (VCO A/B) | max 1 V |
| WET OUT L/R | max 2 V |
| EG (envelope A/B) out | 0…+8 V |
| Drone voice env outs | −10…+10 V |
| LFO A/B out | **0…+10 V unipolar** (by design: drives photo-LEDs) |
| Pulser | ±10 V |
| S&H out | ±5 V |
| Papa Srapa modulator cv out | 0…+12 V |
| Joystick X/Y | −10…+10 V (offset window: left −10…0, mid −5…+5, right 0…+10) |
| Sequencer CV / gate | 0…+5 V / 0…+10 V |
| Env follower env / gate | 0…+10 V / 0…+8 V |
| Keyboard V/oct / gates / pressure | 0…+8 V (8 oct) / 0…+10 V / 0…+8 V |
| Keyboard clock/reset in | 0…+5 V recommended |
| CV inputs tolerate | −10…+12 V |
| Preamp | 1 MΩ in, up to 40 dB gain |

## 4. Behavior notes for DSP

- **Classic generators**: free-running saws (negistor circuits), factory ranges
  gen1 **C0–G2**, gen2 **B1–G3**, gen3 **D3–B4**, gen4 **E4–C6**, gen5
  **G5–D7** (≈8 octaves, unit-to-unit variance). **Stable when MOD off** — the
  movement is beating, not drift. VOLT: 0–~50 % transposes all 5 down; beyond,
  generators cross-modulate (supply-coupling FM) — the dirty zone.
- **Photo-sensor**: red LED + LDR; LED driven by whatever CV reaches the voice
  (LFOs are unipolar for this); ambient light adds in; mains flicker audible.
  Vactrol-style asymmetric response (fast up, slow down). *Implementation
  decision (M3): the CV jack always drives the LED — i.e. patched CV reaches
  the MOD'd generators only through the opto's lag and unipolar clamp; there
  is no direct/bypass CV path on a classic voice.*
- **Papa Srapa modes**: (1) FM+AM off = plain drone; (2) FM = square pitch
  wobble (depth MOD, rate RATE÷DIVIDER); (3) AM = chopped tone at RATE;
  (4) both = chopped + wobble (sirens/birds/bursts); (5) noise switch = white
  noise replaces osc. Audio osc ≈ C0–E7.
- **VCO**: AS3340 triangle core; morph zones saw↔inverted-saw and sine↔tri;
  square + PWM; sub = square −1 oct; hard sync (A); linear FM is
  non-through-zero; TUNE spans 1 octave. ADSR: HOLD = VCA open; LOOP =
  self-retrigger (env-as-LFO) when A and R are between min and ~9 o'clock.
- **Filter**: 12 dB Polivoks type, "dirty and angry", resonance does **not**
  thin bass, L/R audibly slightly different (intentional live stereo). BP/LP
  switch clicks audibly on the hardware (documented as normal).
- **Effector**: Spin **FV-1** per channel (32 kHz, ~1 s delay RAM, 128-instr
  programs; cartridges = 24LC32A EEPROM, 3 programs in FV-1 slots 5/3/7).
  Program persists in the chip after cartridge removal until another load.
  X/Y/Z/BLEND/MASTER shared by both channels; X/Y/Z CV −10…+10 V.
- **Keyboard**: see §6 of `06-manual-digest.md` for modes; pressure = covered
  area; twin behaviour repurposes PRESSURE jack as right-side V/oct; internal
  clock 10–300 BPM with auto-switch to external; presets A–D (all params except
  tempo); calibration menu exists on hardware (not recreated).

## 5. FX cartridge catalog

Full X/Y/Z map for all 13 cartridges: see the table in
[`06-manual-digest.md`](06-manual-digest.md) (§FX cartridge catalog).
Starter set for the recreation: **CATHEDRAL, TIME, VIBROTREM, OCHRE**
(the 42F/N manuals spell it "ORCHE"; the reference card in
`reference-docs/elta-cartridge-ochre-reference-card.pdf` uses "OCHRE" — we use
OCHRE). Remaining 9 = backlog: MAGIC, FILTER, VIBE, PITCH SHIFTER, INFINITY,
STRING RINGER, SYNTEX-1, DIGITAL, GENERATOR.

## 6. NOT specified by the manuals → `TuningConstants.h` (tune by ear, M8)

1. All envelope times (drone ATT/RLS, VCO ADSR min/max) and curve shapes.
2. LFO A/B absolute Hz ranges (only "slow"/"fast" + x1/x6/x10) and pulser range.
3. Filter cutoff range, exact slope behavior, resonance/self-oscillation point.
4. Papa Srapa RATE range, DIVIDER ratios, exact x10 multiplier.
5. VOLT transpose depth (octaves) and dirty-zone onset/curve.
6. Photo-sensor transfer curve and LDR timing.
7. Portamento / vibrato / pressure rise-fall actual times (0–255 / 0–127 units).
8. All effector algorithm internals (only X/Y/Z parameter names are documented).
9. DRY OUT L/R exact position on the 42N panel; number of VCO wave-out jacks
   on 42N (base 42 text mentions "six waveform outputs" — 42N consolidates
   behind the morph knob; we model the 42N render: one osc out on VCO B).

## 7. Decisions log (2026-07-03, user-approved)

Platform **JUCE 8 C++ (Standalone + AU + VST3, macOS first)** · revision
**42N** · **full virtual patch cables** with hardware normalling · effector =
**fixed-point FV-1 VM** + in-house SpinASM programs approximating the
cartridges (real ROMs are proprietary) · FX starter set + framework · every
by-ear estimate isolated in `TuningConstants.h` · faithful behavior first,
conveniences layered on. Implementation plan: `app/` (see repo).
