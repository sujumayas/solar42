/**
 * Headless bounce of ONE Solar 42–style drone example, for the
 * experiments/solar42 investigation. Run:
 *
 *   npx tsx experiments/solar42/bounce-example.ts
 *
 * It reuses the app's own proven conventions (from scripts/build-synth-patches.ts):
 *   - integer-cycle detuning so the loop is (near-)perfectly periodic; the native
 *     engine's crossfade looping removes any residual seam,
 *   - band-limited additive saws,
 *   - canonical instrument.json round-tripped through the REAL parser.
 *
 * Unlike build-synth-patches (which writes into the repo's bundled samples/),
 * this writes the pack into the app's USER library so the main repo is untouched:
 *   ~/Library/Application Support/ToneMatrixSynth/instruments/solar42-drone/
 * plus record copies into experiments/solar42/{renders,patches}/.
 *
 * The interactive, high-fidelity design surface is drone-lab/index.html; this
 * script just guarantees one installable example exists (see 04-drone-lab.md).
 */
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import * as assert from 'node:assert';

import { encodeWav } from '../../src/renderer/src/audio/wav';
import { instrumentToJson, parseInstrumentJson } from '../../src/shared/instrumentJson';
import { Instrument } from '../../src/shared/types/instrument';
import { normalizePeak } from '../../scripts/dsp';

const SR = 44100;
const DURATION_S = 8; // 0.125 Hz freq resolution → slow beats + integer-cycle loop
const LOOP_START_S = 0.01; // room for the engine's 256-sample crossfade
const BASE_PEAK_DB = -1;
const MAX_PARTIAL_HZ = 5500; // band-limit ceiling for saw partials at root

const GRID = 1 / DURATION_S; // 0.125 Hz — every freq must be a multiple of this
const snap = (hz: number) => Math.round(hz / GRID) * GRID;

/** Root A2 = 110 Hz exactly. Chord = root, fifth, octave (a stable cinematic bed). */
const ROOT_NOTE = 'A2';
const CHORD_TONES = [110.0, snap(164.81), 220.0]; // A2, ~E3, A3  (all snapped to grid)

/** Per chord tone, a unison stack of saws detuned by these Hz offsets (on-grid). */
const DETUNE_OFFSETS_HZ = [-0.5, -0.25, 0, 0.25, 0.5];
/** Relative loudness per chord tone (lower tones louder → weighted low end). */
const TONE_LEVEL = [0.5, 0.36, 0.3];

interface Osc { freq: number; level: number; phase: number; }

function buildOscs(): Osc[] {
  const oscs: Osc[] = [];
  let idx = 0;
  CHORD_TONES.forEach((tone, ti) => {
    DETUNE_OFFSETS_HZ.forEach((off) => {
      // A fixed per-oscillator phase (deterministic, golden-ratio spread) is
      // unchanged after a whole period, so the loop stays integer-cycle seamless
      // — but the saws no longer all "reset" together at t=0, removing the
      // aligned edge/buzz at the loop boundary.
      const phase = ((idx++ * 0.6180339887) % 1) * 2 * Math.PI;
      oscs.push({ freq: snap(tone + off), level: TONE_LEVEL[ti] / DETUNE_OFFSETS_HZ.length, phase });
    });
  });
  return oscs;
}

/** Band-limited additive saw: partials 1..K at 1/k amplitude, K capped ≈ 5.5 kHz. */
function renderSaw(osc: Osc, out: Float32Array): void {
  const w = (2 * Math.PI * osc.freq) / SR;
  const partials = Math.max(1, Math.floor(MAX_PARTIAL_HZ / osc.freq));
  const norm = (2 / Math.PI) * osc.level;
  for (let k = 1; k <= partials; k++) {
    const amp = norm / k;
    for (let i = 0; i < out.length; i++) out[i] += amp * Math.sin(w * k * i + osc.phase);
  }
}

function renderBase(oscs: Osc[]): Float32Array {
  const buf = new Float32Array(SR * DURATION_S);
  for (const osc of oscs) {
    assert.ok(
      Math.abs(osc.freq * DURATION_S - Math.round(osc.freq * DURATION_S)) < 1e-9,
      `${osc.freq} Hz is not on the ${GRID} Hz integer-cycle grid`,
    );
    renderSaw(osc, buf);
  }
  return normalizePeak(buf, BASE_PEAK_DB);
}

/** Canonical synth-sampler instrument (shape mirrors scripts/build-synth-patches.ts). */
function buildInstrument(): Instrument {
  return {
    name: 'solar42-drone',
    title: 'Solar42 Drone',
    category: 'synth',
    type: 'synth-sampler',
    gain: 1.0,
    supportsPump: true,
    userCreated: true,
    // Slow swell + long tail. NOTE: the sequencer gate still auto-releases ~1
    // beat after trigger today, so hold/retrigger + this long release + reverb
    // are what sustain the drone (see 03-engine-gap-analysis.md).
    envelope: { attack: 1.2, hold: 0, decay: 0, sustain: 1, release: 4.0 },
    effects: [
      { type: 'chorus', timing: 0, paramsXY: { x: 0, y: 0 }, params: { rate: 0.3, depth: 0.5, mix: 0.5 } },
      { type: 'reverb', timing: 0, paramsXY: { x: 0, y: 0 }, params: { size: 0.7, mix: 0.42 } },
    ],
    samples: [
      {
        class: 'sample',
        sampleType: 'note',
        file: 'base.wav',
        note: ROOT_NOTE,
        gain: 1,
        loop: true,
        time: { start: 0, end: DURATION_S },
        loopTime: LOOP_START_S,
      },
    ],
    noteModulation: {
      filters: [{ type: 'lowpass12', cutoff: 900, resonance: 0.55, drive: 0.35 }],
      modulators: [
        // glacial cutoff sweep (±~0.5 octave) — motion on top of the beating
        { lfo: { wave: 'triangle', gain: 0.12, offset: 0, rate: 0.05 }, destination: { 'filter-cutoff': 1 } },
        // tiny pitch drift (~±7 cents)
        { lfo: { wave: 'sine', gain: 0.006, offset: 0, rate: 0.08 }, destination: { pitch: 1 } },
      ],
    },
  };
}

function writePack(dir: string, wav: Uint8Array, json: string): void {
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'base.wav'), wav);
  fs.writeFileSync(path.join(dir, 'instrument.json'), json);
}

function main(): void {
  const oscs = buildOscs();
  const mono = renderBase(oscs);
  const wav = encodeWav({ sampleRate: SR, length: mono.length, duration: mono.length / SR, channels: [mono] });

  const instrument = buildInstrument();
  const json = instrumentToJson(instrument);
  // Gold check: the emitted JSON must survive the REAL parser unchanged.
  assert.deepStrictEqual(parseInstrumentJson(json), instrument, 'instrument.json is not canonical');

  const here = __dirname;
  const userLib = path.join(
    os.homedir(),
    'Library/Application Support/ToneMatrixSynth/instruments/solar42-drone',
  );
  const rendersDir = path.join(here, 'renders');
  const patchesDir = path.join(here, 'patches');
  fs.mkdirSync(rendersDir, { recursive: true });
  fs.mkdirSync(patchesDir, { recursive: true });

  // 1) install into the USER library (outside the repo) → playable in the app
  writePack(userLib, wav, json);
  // 2) record copies inside the experiment
  fs.writeFileSync(path.join(rendersDir, `Solar42Drone_${ROOT_NOTE}_${DURATION_S}s.wav`), wav);
  fs.writeFileSync(path.join(patchesDir, 'solar42-drone.instrument.json'), json);

  const beats = DETUNE_OFFSETS_HZ.filter((o) => o !== 0).map((o) => `${Math.abs(o)}Hz`);
  console.log('✓ Bounced Solar42 Drone');
  console.log(`  oscillators : ${oscs.length} saws (${CHORD_TONES.length} tones × ${DETUNE_OFFSETS_HZ.length})`);
  console.log(`  root/loop   : ${ROOT_NOTE}, ${DURATION_S}s seamless loop, beats @ ${[...new Set(beats)].join(', ')}`);
  console.log(`  filter/mods : lowpass12 900Hz res .55 drive .35 + slow cutoff LFO + pitch drift`);
  console.log(`  fx          : chorus → reverb`);
  console.log(`  installed   : ${userLib}`);
  console.log(`  record      : ${path.relative(process.cwd(), rendersDir)}/  +  ${path.relative(process.cwd(), patchesDir)}/`);
  console.log('  → relaunch the app; "Solar42 Drone" appears in the user library.');
}

main();
