# Sources (reference-only)

We use these to understand the *architecture and recipe*. We synthesize original
audio; **no third-party sample content is bundled** (per the house guardrails).

## The instrument
- Elta Music official Solar 42 page — https://www.eltamusic.com/solar-42
- Reference demo: *SOLAR 42 | SOUND DEMO 3* (ELTA music) — https://youtu.be/dzU8T2lKUhs
- Perfect Circuit product page (Solar 42N) — https://www.perfectcircuit.com/elta-solar-42.html

## Official manuals (downloaded 2026-07-03 → `reference-docs/`, digested in `06-manual-digest.md`)
- **Solar 42 user manual** (28 pp) — https://www.eltamusic.com/_files/ugd/12d408_d4c9881033524945ae276dc3559589ff.pdf
  → `reference-docs/solar42-user-manual-elta-official.pdf` (+ extracted `solar42-manual.txt`)
- Solar 42F manual — https://www.eltamusic.com/_files/ugd/64d4dc_eaf864218c9141189e277fb80dbcd476.pdf
- Solar 42N manual — https://www.eltamusic.com/_files/ugd/12d408_5492a3f213ac47ca90e614c44c40f2e6.pdf
- Solar 50 owner's manual (predecessor, drone-voice lineage) — https://www.eltamusic.com/_files/ugd/12d408_0246d69f6e8f465bae79c78dc086aa7c.pdf
- **Cartridge DIY kit** (reveals effector = Spin FV-1, programs in 24LC32A EEPROM) —
  https://www.eltamusic.com/_files/ugd/12d408_0a22d000bb234e0a9345aa20e3e76bcb.pdf
- Cartridge catalog (X/Y/Z maps) — https://www.eltamusic.com/_files/ugd/12d408_08330024fc0049398a6aa0810b4d3539.pdf
- Solar keyboard quick reference (Juno CDN) — https://imagescdn.juno.co.uk/manual/960809-01U.pdf
- FV-1 DSP docs (for modelling the FX): SpinSemi — http://www.spinsemi.com/ ;
  SpinCAD Designer block library — https://github.com/HolyCityAudio/SpinCAD-Designer

## Architecture / press
- SynthAnatomy, Superbooth 23 first look — https://synthanatomy.com/2023/09/superbooth-23-elta-music-solar-42-a-microtonal-polyphonic-ambient-machine-first-look.html
- Gearnews preorder writeup — https://www.gearnews.com/elta-music-solar-42-microtonal-polyphonic-ambient-machine/
- Geeky Gadgets overview — https://www.geeky-gadgets.com/ambient-drone-synthesizer/

## Software precedent (proves the sound is reachable with basic blocks)
- VCV Rack "ELTA Solar 42 Emulator v2" thread — https://community.vcvrack.com/t/elta-solar-42-emulator-v2-and-challenge/21906

## Drone / Polivoks technique (cross-refs, house docs)
- `docs/CLASSIC_SOUNDS_INVESTIGATION.md` — drones = targets #11–12; distilled
  Polivoks recipe (2 detuned pulse osc, cross-mod/noise, hot 12 dB LP, S&H drift).
- `docs/SOUND-DATA-MODEL.md` — instrument JSON schema; §239-242 unison/2nd-osc roadmap.
- `docs/EFFECTS_INVESTIGATION.md` — reverb/delay/chorus algorithm references.

## Web Audio building blocks (for the Drone Lab prototype)
- MDN Web Audio API (OscillatorNode, BiquadFilterNode, OfflineAudioContext) —
  https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API
