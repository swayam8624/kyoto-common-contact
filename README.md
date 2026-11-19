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

### Phase 18 next: same-snapshot A/A noise floor

The complete Mac **eleven-algorithm** natural-material ranking has been
reported and archived in
[the Phase 18 Mac evidence note](docs/CBP_PHASE18_MAC_ACTUAL_2026-11-19.md).
V4 ranks first (geomean Guard/V4 1.0306x, 21/36 wins), but this
approximately 3% apparent benefit has **not** been shown statistically
robust against timing drift. The three scene seed values vary only
forcing phase, not initial topology.

The additive, strictly optional same-code replication protocol in
[docs/CBP_PHASE18_AA_REPLICATION_PROTOCOL.md](docs/CBP_PHASE18_AA_REPLICATION_PROTOCOL.md)
times two independently instantiated Guard policies and two independently
instantiated, **unchanged** V4 policies per original snapshot. It replays
each saved natural scene under several randomized method orders; no
mass-spring simulation or exporter runs again. Source snapshot SHA-256,
original replay CSV SHA-256 and original-versus-new SAP candidate
fingerprints are rechecked.

After pulling and rebuilding only ccp_material_replay, reproduce with:

```bash
python3 scripts/recheck_kyoto_phase18_aa.py \
  --results output/kyoto-decision-mac-first \
  --replayer build/kyoto-final-cpu/ccp_material_replay \
  --output output/kyoto-phase18-aa-mac \
  --repeats 4
open output/kyoto-phase18-aa-mac/AA_RECHECK.md
```

Linux [A/A regression and source-fingerprint CI](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44455846122)
passed. One 512-AABB test showed identical frozen V4 copies differing by
1.4222x **without a backend-action difference**, emphasizing that tiny
one-run speedups demand stability controls. This specific Linux variance
is not asserted to hold on the Mac.

### Original 144 Mac A/A timings and corrected scheduling-label audit

The Mac completed 144 same-snapshot timed replay runs, reporting natural
Guard/V4 1.0188x and rapid-cloth N=1058 1.2139x. The unweighted
geometric mean across the remaining 11 natural conditions is ~1.0027x,
indicating near-parity outside that one fixture family. The full results,
exact limitations, and an instrument-order correction are preserved in
[the Mac A/A evidence note](docs/CBP_PHASE18_MAC_AA_RESULTS_2026-11-19.md).

**Measurement caveat:** the ORIGINAL optional A/A instrumentation assigned
copy A/B labels *after* the shuffled Kind order, making A always timed
before B. Its timing-disagreement ratios are descriptive but NOT
balanced early/late label controls. Fixed code now binds identities BEFORE
random shuffling. The original old Mac output remains frozen and the
new replay requires capability aa_labels_shuffled_independently.

To inspect the old 144 Mac replays without timing ANYTHING again:

```bash
python3 scripts/analyze_kyoto_phase18_aa_robustness.py \
  --aa output/kyoto-phase18-aa-mac/AA_RECHECK.json \
  --output output/kyoto-phase18-aa-original-robustness \
  --draws 2000
open output/kyoto-phase18-aa-original-robustness/AA_ROBUSTNESS.md
```

This source-hash-verified report audits A/B execution slots, prints the
actual V4 action-disagreement frame indices from the old CSVs, and
calculates balanced-condition leave-one-out and fixed-scene resampling
sensitivity. Resampling conditional on fixed geometry is not a
population confidence interval and does not establish physical solver speed.


### Phase 18 post-hoc Mac A/A robustness: source-verified condition sensitivity

The original 144 repeated Mac A/A replays were reanalyzed **without any
new native timings**. The source-verified report and seven exact
backend-decision disagreement frames are preserved in
[the full Mac A/A post-hoc robustness note](docs/CBP_PHASE18_MAC_AA_POSTHOC_ROBUSTNESS_2026-11-19.md).

Full fixed natural suite: Guard/V4 **1.0188x**, conditional time-repeat
resampling **1.0117x..1.0257x**. Excluding the strongest previously
observed cloth/rapid/N=1058 condition: **1.0027x**, conditional
resampling **0.9954x..1.0106x**. The positive rapid-cloth condition:
**1.2139x**, conditional **1.2042x..1.2255x**. None of these conditional
resampling envelopes should be presented as a population confidence
interval or a claim on independently generated physical geometries.

The original A/B label chronology was biased: Guard A and V4 A ran
before their B duplicates in **all 144/144** replays. This limitation
remains attached to the old evidence even though later *optional*
instrumentation correctly randomizes copy identities beforehand.
The aggregate Guard/V4 ratio over both copies does not depend
mathematically on which identical copy was named A; it still has
whole-trajectory method-order, thermal and fixed-fixture limitations.

In sponge baseline N=1331 seed3203 repeat2, the copies differed on
frames 11..17: A used SAP (median 1.800 ms) while B used Guard
(median 0.135 ms), including two >2-ms Guard outliers. The saved
CSV cannot show the controller's internal rebuild streaks/EWMAs or
a counterfactual optimal backend. Pair correctness remained verified.
No changes to frozen V4 were made. Further research requires actual
independently varied geometry/topology and end-to-end solver evidence.


## Kyoto independent geometry/topology and real-contact solver gate

After Phase 18 Mac revealed Guard/V4 near parity on most fixed
geometries, this branch now contains a NEW, additive held-out
material/solver research protocol. Do not retune frozen V4 based on
those previous results.

The independent material fixture code now has separate motion, initial
vertex geometry and topology seeds. Cloth triangle diagonals, actual
physical spring constraints, anchor layout and volumetric crossbracing
can change. Historical zero-variation fixtures and binary KYOTOM01
replay semantics are preserved exactly.

The existing MultiCloth coupled solver ALSO supports opt-in
independent physical vertex jitter, triangle diagonal connectivity,
springs and anchor layouts, with **actual nonzero narrow-phase
contact tests and sphere-contact responses**. Original SAP, Guard
and frozen V4 solve the exact same source geometry per variant,
and exact frame-by-frame solver vertex state equality to SAP is
enforced. All original policies remain available.

[Complete experimental instructions](docs/KYOTO_HELDOUT_GEOMETRY_COUPLED_PROTOCOL.md)
and [actual source-verified Linux native evidence](docs/KYOTO_HELDOUT_FIRST_NATIVE_RESULT_2026-11-19.md).

**First Linux results and essential negative outcomes:** independent
4-anchor and 6-anchor actual cloth fixtures with 48 frames showed
many sphere–cloth contact responses and exact SAP/Guard/V4 solver
mesh agreement, with zero collapsed triangles, but **zero nonlocal
cloth self-contact responses** and **zero V4 SAP actions/trials/
switches** (Guard all 240 queries per scenario). This is genuine
coupled-contact correctness evidence, NOT an adaptive full-solver
speedup or self-folding validation. Native CI:
[verified held-out experiment](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44465880693).

Build on the Mac, using a fresh output directory:

    cd ~/Desktop/Programming/kyoto-common-contact
    git switch research/kyoto-decision-coverage
    git pull --ff-only origin research/kyoto-decision-coverage
    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_export ccp_material_replay ccp_kyoto_heldout_fixture_tests ccp_kyoto_coupled_variation_tests ccp_kyoto_heldout_coupled
    build/kyoto-final-cpu/ccp_kyoto_heldout_fixture_tests
    build/kyoto-final-cpu/ccp_kyoto_coupled_variation_tests
    python3 scripts/run_kyoto_heldout_research.py --exporter build/kyoto-final-cpu/ccp_material_export --replayer build/kyoto-final-cpu/ccp_material_replay --coupled build/kyoto-final-cpu/ccp_kyoto_heldout_coupled --output output/kyoto-heldout-mac-first
    python3 scripts/test_kyoto_heldout_research.py output/kyoto-heldout-mac-first
    open output/kyoto-heldout-mac-first/HELDOUT_RESULTS.md

This default matrix generates 54 truly different material
geometry/topology AABB sequences (three material families, two regimes,
three budgets including the critical 1024 budget, three new seeds)
and nine independently seeded
free/pull/fold coupled physical solver cases (three initial meshes,
three drive scenarios). Reported original-policy query times are
separate from physically coupled full-step times. The output stores
immutable source geometry hashes and solver meshes.

Render any real solver visual comparison into a looping GIF (requires
Pillow, optionally ffmpeg for MP4), keeping method geometry identical:

    python3 scripts/render_kyoto_visuals.py solver --input-dir output/kyoto-heldout-mac-first/coupled/fold-geom73013-topo86011 --output output/kyoto-heldout-mac-first/coupled/fold-geom73013-visual --fps 9 --stride 1

Exact source snapshots, not fabricated artistic cloth deformation,
drive the visuals. Never infer CCD, guaranteed non-penetration,
successful self-intersection avoidance or a new V4 speedup from
the baseline smoke pass.

### Source-verified six-family held-out Linux results (November 2026)

The expanded native rapid-motion CI proof covers **54 independently
seeded geometrical/topological scenes**, SIX material families and ten
actually executed CPU broad phases. See
[the full measured 54-scene result](docs/KYOTO_HELDOUT_54_SCENE_NATIVE_RESULT_2026-11-19.md)
and [its passing GitHub native CI](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44468375567).

Frozen V4 Guard-relative geomean is **1.0552x** with only **25/54**
scene wins: not general superiority. Larger rapid-cloth N1058
1.2853x, soft-ball N1024 1.2601x/N2116 1.3300x, and spring
N504 1.2018x/N1008 1.2230x are promising workload-specific
conditions. Other material regimes often tied or regressed.

All **32400 CPU method-frame AABB candidate signatures**, the
144 archived source-file SHA-256 receipts, and the separate coupled
SAP/Guard/V4 physical state comparisons passed independent
post-download checks. On naturally driven coupled solver scenes,
V4 stayed on Guard for all 2700 broad-phase iterations; no SAP
trial or switch occurred, and no naturally driven nonlocal cloth
self-contact response occurred. An isolated *constructed* self-overlap
fixture did produce real bounded self-contact correction on all three
independent meshes (283, 276, 317 responses). These are different
scientific claims and must not be conflated.

For the same ten-policy post-analysis from the original full Mac
output, with NO native rerun:

    python3 scripts/analyze_kyoto_heldout_evidence.py --results output/kyoto-heldout-mac-first --output output/kyoto-heldout-mac-first/full-method-scores --warmup 8
    open output/kyoto-heldout-mac-first/full-method-scores/HELDOUT_ALL_METHODS.md


### First completed Apple Silicon held-out run (54 scenes)

The user executed the default **three-family (cloth/sponge/mixed),
baseline+rapid, three-budget, three-seed** 54-scene suite on the Mac.
Build, export, all original replay methods, 54 source integrity
and framewise SAP candidate audits, and nine 60-frame coupled
solver parity runs completed. The original Mac console contained
54 Guard/V4 values, not the actual full-method output contents.

[Mac console-derived, explicitly qualified first-result audit](docs/KYOTO_HELDOUT_54_SCENE_MAC_FIRST_2026-11-19.md).

From those 54 four-decimal printed ratios, the Guard/V4
geometric mean is **0.9992x** with **27/54 wins**. Rapid cloth
N1058 retained **1.1146x** over the three new meshes; rapid
sponge N1331 declined to **0.9003x**, partly driven by a
single 0.7388x negative observation. Full baseline subset
1.0101x, rapid subset 0.9883x.

These are single-session descriptive Mac query timings, not
balanced time repeats or solver time. The earlier 1.0552x
Linux six-family RAPID-only suite has a DIFFERENT
material/regime matrix; do not directly compare the overall
figures as if they replicated identical scenarios.

The complete all-method ranking (including any native
Bullet DBVT result actually executed) and controller decision
counts are in the local Mac file generated by the command:

    cat output/kyoto-heldout-mac-first/full-method-scores/HELDOUT_ALL_METHODS.md

The underlying Mac raw files remain locally stored and
are NOT represented as uploaded to the repository. Do not
infer any missing method or source values.
