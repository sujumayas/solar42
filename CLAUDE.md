# Solar 42N — digital instrument project

Digital recreation of the **Elta Music Solar 42N** synthesizer as a native
**JUCE 8 C++** app/plugin (Standalone + AU + VST3, macOS first). The buildable
product lives in `app/`; everything else at root is research/reference docs.

## Read first (in this order)

1. `08-implementation-plan.md` — **approved plan of record + milestone status
   tracker** (the durable cross-session state; keep it updated).
2. `07-42n-panel-inventory.md` — hardware source of truth: 42N voice naming,
   full control/jack census, normalled connections, voltage table, and the
   list of values the manuals do NOT specify.
3. `00-LOG.md` — chronological log incl. listening verdicts (newest at bottom).
4. `09-calibration-protocol.md` — the M8 listening protocol: calib scenes →
   constants → verdict table (kit: `app/tools/calib`, WAVs in `renders/calib/`).
5. `06-manual-digest.md` — manual digest (base-42 voice numbering; 07 wins on
   42N specifics). `01`–`05` + `drone-lab/` are historical (early web
   experiment — do not extend it).

## Build / test / run

- Gate: `app/scripts/check.sh` — configure + build + ctest + pluginval +
  render smoke. **Must be green before any milestone is called done.**
- Standalone app: `app/build/Solar42N_artefacts/RelWithDebInfo/Standalone/Solar42N.app`
- Audition renders: `app/build/solar42n_render <out.wav> <seconds>` → put
  keeper WAVs in `renders/`.
- Do not `git push` — commits stay local unless the user pushes.

## Engine conventions (do not break)

- All jack signals are **volts** at audio rate through `VoltBus`; jack IDs in
  `engine/Jacks.h` are **frozen** (registry is the single source of truth for
  engine, UI, and state).
- `dsp/`, `fv1/`, `engine/` static libs stay **JUCE-free** (unit-testable).
- Every by-ear estimate goes in `dsp/TuningConstants.h` with a comment — never
  scatter magic numbers; M8 calibrates that one file.
- Fixed module process order = hardware causality; backward patches read the
  previous 64-sample sub-block (intended feedback semantics).
- L/R and multi-instance components pull fixed offsets from `Tolerances`
  (persisted unit serial) — the "live stereo" philosophy, everywhere.

## End-of-feature ritual (run this EVERY time a feature/milestone lands)

1. `app/scripts/check.sh` green (tests + pluginval + render).
2. Update the **milestone status table** in `08-implementation-plan.md`
   (status, commit hash, evidence; adjust upcoming milestones if scope moved).
3. Add a dated entry to `00-LOG.md` — what landed, what changed sonically,
   any user listening verdict (PASS/ITERATE style).
4. Update the session task list (TaskCreate/TaskUpdate) to mirror the table.
5. If the sound changed: render a fresh audition WAV into `renders/` and ask
   the user for an ear check.
6. Prune/correct any doc the change made stale (esp. 07/08).
7. Commit locally with a descriptive message (no push).

## Listening protocol

The user is the final gate on sound. Criteria are still being co-developed
(first verdict 2026-07-03: standalone app > offline render; both promising).
Until formal criteria exist in M8, always ship an audition artifact with
sonic changes and log the verdict in `00-LOG.md`.
