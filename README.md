# Kyoto Collision Broad-Phase — validated CPU core and visual evidence

An exact, stateful broad-phase candidate generator for deformable-contact
research, written in **C++23**. The selected implementation is the **V4
single-backend Guard–SAP Bounded Trial**. It has been benchmarked repeatedly
against Guard, SAP, regional methods, an ML selector, Bullet DBVT and
exploratory BVHs. It is **not** a continuous collision detector and the
standalone V4 CPU algorithm has **not** been shown to improve the complete
coupled cloth solver's time or physical fidelity.

## Final measured result — no re-tuning required

Across the independent 864-scene V6 confirmation (three timing repetitions
per scene), Guard/bounded-trial geometric-mean speedup was **1.083×**,
summed-time speedup **1.317×**, and **596/864** scene configurations had lower
mean query times. Full method-by-method results and negative findings remain
in the historical V5/V6 research branches. The failed V6 unified hierarchy
was **not** promoted. See [FINAL_RELEASE.md](FINAL_RELEASE.md).

## Run the visual and physical-quality package

On macOS with CMake, Apple Clang and Python 3:

```bash
python3 -m pip install Pillow
# Optional for a matching looping MP4: brew install ffmpeg
bash scripts/run_kyoto_visual_package.sh output/kyoto-final-showcase
```

The final V4 controller is also selectable as `Policy::FinalBounded` in
`MultiCloth`. The coupled solver's exported geometry is checked against
SAP on every frame; a timing or fidelity advantage in the complete solver
is **not** assumed.

The runner performs a **short** correctness gate (not another massive
tournament), captures six materials in six motion regimes and all eight
coupled-cloth broad-phase policies, including the final V4 selector, and exports two true-data
looping GIFs with MP4 variants when FFmpeg is available:

- `output/kyoto-final-showcase/videos/all-materials-six-regimes.gif`:
  **six materials × four methods**, cycling across six motion regimes;
  each method shows the same actual 3D AABB geometry but its own CPU time.
- `output/kyoto-final-showcase/videos/all-eight-solver-policies.gif`:
  actual coupled cloth/sphere mesh sequences with verified SAP state parity.
- `output/kyoto-final-showcase/reports/SOLVER_QUALITY.md`:
  edge strain, nearly collapsed triangles, contact-response diagnostics and
  physical-state equivalence.
- `output/kyoto-final-showcase/reports/REPLAY_SCORES.md`:
  exact-pair-verified timings for a fixed representative replay.
- `output/kyoto-final-showcase/SHOWCASE_MANIFEST.json`:
  reproducibility manifest and frozen geometry SHA-256 hashes.

**Important:** Exact broad-phase methods returning the same candidate pairs
should produce the **same physics** under an identical solver. They cannot
honestly be credited with fewer cloth folds merely because they are faster.
Full self-intersection/CCD checks and realistic physical-material calibration
remain separate solver-quality requirements.

For every benchmark mode, external data importer, source, limitations, and
commands, read [the benchmark/visual documentation](docs/BENCHMARK_VISUAL_RELEASE.md).

## Build the final CPU API without rebuilding the archive

```bash
cmake --preset kyoto-final-cpu
cmake --build --preset kyoto-final-cpu
ctest --preset kyoto-final-cpu
```

The public C++ header is
[`include/ccp/final_broadphase.hpp`](include/ccp/final_broadphase.hpp).
It exposes `GuardSAPBoundedTrial` and `PersistentSAP`.
One instance owns the state for one ordered stable-ID AABB stream;
returned pairs are sorted and unique, with inclusive AABB contact semantics.
The validated native replay and exhaustive small-scene pair oracle remain in
the test suite.

## External, more credible benchmark inputs

* **IPC Toolkit's official LBVH:** a vital published external family, but
  **not** yet integrated into the generic AABB replay; IPC's edge/face
  candidate semantics and mesh adjacency filtering must first be matched.
* **ARCSim/CLOTH3D fixed-topology OBJ sequences:** supported through
  `benchmarks/import_mesh_sequence.py`; dynamic tearing/remeshing rejected.
* **RGBench and other real point-cloud observations:** explicit world-frame
  registration/synchronization required; PCD/PLY-to-OBJ surface RMS
  evaluation available in `benchmarks/compare_reference_geometry.py`
  but **not** the official benchmark score.

Consult [docs/BENCHMARK_VISUAL_RELEASE.md](docs/BENCHMARK_VISUAL_RELEASE.md)
before comparing physical and algorithmic metrics.

## Repository map

| Area | Purpose |
|---|---|
| `include/ccp/final_broadphase.hpp` | Frozen measured V4 core selector |
| `src/`, `include/ccp/`, `native/` | Physical solver and interactive native scene |
| `apps/material_replay_benchmark.cpp` | Audited identical-snapshot CPU comparison |
| `apps/visual_multicloth_export.cpp` | Real solver mesh evidence exporter |
| `benchmarks/` | External OBJ import, pair correctness and physical fidelity adapters |
| `scripts/render_kyoto_visuals.py` | Genuine synchronized mesh/AABB GIF+MP4 renderer |
| `scripts/run_kyoto_visual_package.sh` | One canonical reproducible showcase command |
| `tests/` | Pair oracle and evidence regression checks |
| `docs/` | Complete protocol, release, research and historic documentation |

## Cleanup and archival

This branch removes **4,308 tracked generated/vendor files (~239.5 MB)**
from the active tree, including a checked-in presentation Python
environment, duplicated slideshow intermediates and giant raw PPM
framebuffers. The **original validated release and research branches
preserve all historical files**. Old C++ sources that still have CMake
targets/tests are not labeled dead and deleted by guesswork. A lean build
via CMake presets avoids compiling those historical programs.

Run `python3 scripts/audit_repository.py --output output/repo-audit` for
an inspectable tracked-file/compilation-reference report, and see
[docs/REPO_CLEANUP.md](docs/REPO_CLEANUP.md).

The original project demo instructions are preserved in
[docs/HISTORICAL_README.md](docs/HISTORICAL_README.md), and the existing
interactive viewer remains available via `open_multicloth_demo.sh`.


## Research track navigator — Online/Guard/ML and Phase 17

The historical **Online, Global Guard, Selective Guard, persistent SAP,
learned ML, regional methods, Bullet and frozen V4** have **not** been
superseded by the Phase 11–16 synthetic pressure-index experiments. Those
experiments answer a different scaling question.

Start with [the complete research map](docs/KYOTO_RESEARCH_UNIFIED_MAP.md)
to understand both tracks, what was measured, and what was not. On the
Phase 17 branch, a single additive comparison command is available:

    bash scripts/run_kyoto_phase17.sh output/kyoto-phase17-mac-01

Then open the **actual measured algorithm trace explorer**, with Global Guard,
SAP, original Online, V4 and the regional methods shown and individually
selectable (plus ML/Bullet if supplied and executed):

    open output/kyoto-phase17-mac-01/reports/KYOTO_METHODS_OFFLINE.html

The associated full-pair results and separate spatial-pressure counts are
also written to `reports/KYOTO_UNIFIED_EVIDENCE.md` and the machine-readable
`reports/KYOTO_UNIFIED_EVIDENCE.json`. The Phase 17 reduced Linux validation
is archived in the [GitHub Actions run](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44422251207).

This runs the historical full-output material algorithms and a separate,
bounded set of synthetic persistent-grid CPU comparisons. It emits actual
pair ID files from *both* persistent and independently rebuilt grids, then
checks every pair, duplicate and complete sorted pair set. Historical models
are only scored if the original native trained weights are supplied. The
new report does not rank unlike-for-unlike timing boundaries, nor claim
solver/CCD or end-to-end performance superiority.

### Mac Phase 17 findings and V4 decision coverage

The 19 November Apple Silicon run is analyzed in
[the Phase 17 Mac results note](docs/CBP_PHASE17_MAC_RESULTS_2026-11-19.md).
All 36 material trajectories used fewer than 384 primitives, so the
frozen V4 algorithm could not enter its SAP trial. The measured
small-scene Global Guard result should not be mistaken for an online
switching study. The newly added post-run decision audit works on the
**existing** CSV files without repeating the benchmark:

    python3 scripts/audit_kyoto_phase17_decisions.py --materials output/kyoto-phase17-mac-01 --output output/kyoto-phase17-mac-01/decision-audit-20261119
    open output/kyoto-phase17-mac-01/decision-audit-20261119/V4_DECISION_AUDIT.md

The report identifies actual chosen backends, V4 trial/switch counts,
ML execution status and the difference between all-frame console means
and warmup-excluded CSV means. Model weights are never invented.

## After the 864-frame V4 no-switch result: controlled decision coverage

The user's Phase 17 Mac evidence showed 36 trajectories below the frozen
384-AABB eligibility floor: V4 executed Guard on all 864 frames, with no SAP
trial. That establishes a small-scene negative result, not an evaluated
adaptive policy. [The new decision-coverage experiment](docs/CBP_DECISION_COVERAGE_EXPERIMENT.md)
does not modify frozen V4.

A separate repeatable experiment now exports genuine 512/1024-budget
mass-spring snapshots across multiple seeds, verifies actual primitive
counts, and randomizes the order of *all original full-output algorithms*
while independently checking SAP candidate pairs. A distinct artificial
rigid-translation case tests whether V4's bounded SAP trial can activate;
its results never enter the physically stepped material ranking.

Start on this research branch using a new output directory:

```bash
cmake --preset kyoto-final-cpu
cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_export ccp_material_replay
python3 scripts/run_kyoto_decision_coverage.py \
  --exporter build/kyoto-final-cpu/ccp_material_export \
  --replayer build/kyoto-final-cpu/ccp_material_replay \
  --output output/kyoto-decision-mac-first
```

Read `DECISION_COVERAGE.md`: each source-derived backend count, SAP trial
frame and switch is reported explicitly, rather than inferring activation
from scene size. No trained ML result is invented when the native weights
are absent. Linux CI: [random-order and size-eligible V4 regression](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44431580036).

### Phase 18 Mac results: 101 natural SAP frames

The 19 November Apple Silicon 39-trajectory evaluation is recorded in
[the source-backed actual Mac results](docs/CBP_PHASE18_MAC_ACTUAL_2026-11-19.md).
Original V4 selected SAP for 101 of 2160 naturally generated frames,
including three sustained SAP adoptions during N=1058 rapid cloth and
two rejected 8-frame trials on N=1331 sponge. Natural Guard/V4 geometric
mean was 1.0306x (21/36 trajectory wins); artificial rigid translation
results remain separate. Trained ML was NOT executed.

The original full pair-count/hash-validated CSVs on the Mac contain all
eleven measured algorithms even though the first decision JSON reported
only Guard/V4 timings. To **score all original methods without rerunning**:

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git pull --ff-only origin research/kyoto-decision-coverage
python3 scripts/analyze_kyoto_decision_coverage.py \
  --results output/kyoto-decision-mac-first \
  --output output/kyoto-decision-mac-first/verified-all-methods-v1
open output/kyoto-decision-mac-first/verified-all-methods-v1/VERIFIED_ALL_METHODS.md
```

The report fails if source SHA-256, execution-order receipts or framewise
candidate **count/hash** records disagree; it does NOT independently compare
raw candidate-ID streams. It separates natural and intentionally artificial
motion. The three seed values vary only the forcing phase of a fixed
material/topology fixture, not initial scene geometry. Linux [CI regression](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44434453461)
passed both valid and deliberately corrupted evidence checks.
