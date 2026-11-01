# Evidence and method invocation audit

This note follows the current checked-in benchmark code. It maps what is run,
what the native viewer exposes, and which work is inside each reported timer.
It does not update recorded measurements. Existing CSV files are measurements
from their original runs; treat their age, build and host as unknown unless the
run's own metadata says otherwise.

## Methods that execute

### Nine isolated broad-phase methods

`apps/broadphase_leaderboard.cpp` loops over every method ID from 0 through 8
for each of six scenes (32 generated snapshots per scene). Each scene's raw
bounds are generated once, copied to the oracle and passed to every method in
the same order. The exact current padded-AABB pair set is prepared before the
timed calls. Each result is checked for required-pair coverage after timing;
an omission aborts the program. `--self-test` additionally checks exhaustive,
SAP, fresh Bullet and persistent Bullet on small fixtures. The script does not
substitute a local stand-in for Bullet.

| ID | CSV method | Actual call path | Native viewer status |
|---:|---|---|---|
| 0 | `exhaustive_aabb` | All-pairs AABB reference candidate generation | Available in single-cloth mode via **B**; serves as a correctness reference |
| 1 | `sweep_prune` | Local sweep-and-prune candidate generation | Available in single-cloth mode via **B**; also the multi-cloth SAP baseline |
| 2 | `bullet_dbvt` | Fresh upstream Bullet `btDbvtBroadphase` construction and query per snapshot | Offline benchmark only |
| 3 | `guarded_sap` | Shared `BroadPhaseService` guarded pair-list reuse | Single-cloth **Q** toggles rebuild/reuse; multi-cloth **Q** selects the multi-cloth global guard |
| 4 | `bullet_dbvt_dynamic` | Persistent upstream Bullet DBVT with proxy updates per snapshot | Offline benchmark only |
| 5 | `region_fixed` | Flat regional candidate blocks, fixed region budget | Multi-cloth region family; **Z** selects fixed budget and **J** selects flat traversal |
| 6 | `region_adaptive` | Flat regional candidate blocks, adaptive budget | Multi-cloth region family; **Z** selects adaptive budget and **J** selects flat traversal |
| 7 | `region_sweep_fixed` | Hierarchical region-envelope sweep, fixed budget | Multi-cloth region family; **J** selects two-tier traversal and **Z** fixed budget |
| 8 | `region_sweep_adaptive` | Hierarchical region-envelope sweep, adaptive budget | Multi-cloth region family; **J** selects two-tier traversal and **Z** adaptive budget |

The isolated leaderboard is a spatial query benchmark: it has no precise
triangle contact resolution, collision response or solver. Its generated
`sheets2`/`sheets3` geometry is not physically coupled. `simulated_cloths3`
uses bounds sampled from a real simulation trajectory, but the benchmark's
timing excludes simulation production and still measures candidate discovery
only. All nine IDs are invoked for all six cases; the output matrix is
54 rows. The CSV summarizer now requires that complete matrix, 32 samples per
row, zero required-pair misses and a consistent primitive count per scene.

### Seven physical policies

`apps/multicloth_benchmark.cpp` loops across three sheet counts and all seven
policy IDs, writing one aggregate row per scene/policy (21 rows). Policies
1–6 are configured explicitly in this driver: fixed/adaptive regional
frontiers, flat/two-tier traversal, and the online controller. Each trajectory
is compared particle-by-particle against a separately advanced full-SAP
reference, with an error at or above `1e-7` failing the run. It enables
`setAuditing(true)` on both the reference and measured simulation throughout.
The research simulator runs its exhaustive AABB coverage oracle on audit
frames (first frame and every 15th frame, in the first and last iterations).
That oracle runs inside `step()` timing. Thus `meanStepMs` is a complete
audited-step cost, not an oracle-free production timing. The paired states and
per-frame oracle make the result useful for correctness and full verified
work cost; the audit overhead must be considered when interpreting speed.

| Driver ID | CSV policy | Configuration in benchmark | Native multi-cloth viewer |
|---:|---|---|---|
| 0 | `sap_rebuild` | Rebuild SAP; reference path is retained for state samples | **Q** cycle; also SAP baseline |
| 1 | `global_guarded` | Global guarded service | **Q** cycle |
| 2 | `region_fixed` | Regional certified, fixed, flat traversal | **Q** cycle; configurable with **Z/J/P** |
| 3 | `region_adaptive` | Regional certified, adaptive, flat traversal | Configurable with **Z/J/P**; reached from the regional family |
| 4 | `region_sweep_fixed` | Regional certified, fixed, hierarchical traversal | Configurable with **Z/J/P** |
| 5 | `region_sweep_adaptive` | Regional certified, adaptive, hierarchical traversal | Configurable with **Z/J/P** |
| 6 | `online_contact_work` | Online contact-aware controller | **Q** cycle and **T** toggle; switches partition/frontier online |

The native multi-cloth viewer is a separate experiment from the single-cloth
viewer. Its **S** key toggles self-collision and its drag interaction changes
the physical workload. `MultiCloth` defaults to self-collision enabled, so the
seven-policy driver includes the default discrete nonlocal self-contact path;
it does not exercise mouse dragging. Self-contact response is common across
the seven policies and therefore covered by the state-equivalence check, but
the policy benchmark does not isolate self-contact cost. Neither fixture
claims CCD or collision-free guarantees.

`apps/online_controller_trace.cpp` is a time-series trace, not another set of
seven comparisons. It runs the online policy against SAP on the same fixed
three-sheet trajectory, checks coordinates every frame and enables the
independent coverage audit on frame 1 and every 15 frames. Since oracle work
is inside the simulator step, audited trace rows include that cost while
non-audited rows do not. Its summary averages the post-warm-up trace as
recorded, so the reported values mix those audit frames. Keep this caveat with
the result; the trace is not a clean oracle-free timing estimate.

`apps/patch_granularity_benchmark.cpp` is a distinct 12-configuration
granularity ablation. It disables audit before the calibration/evaluation
windows and checks state against the SAP trajectory at each frame. Calibration
and later evaluation frames do not overlap; the summarizer chooses the winner
from calibration only.

## The live viewer does not run every isolated method

In single-cloth mode, **B** selects sweep-and-prune or exhaustive AABB,
**Q** selects guarded pair reuse or rebuild, and **V** toggles the independent
oracle. This path uses the shared sphere/triangle service and its solver; it
does not call Bullet DBVT or the isolated region search methods.

In multi-cloth mode, **Q** cycles SAP, global guard, regional certified and
online contact-aware policies. **Z**, **J** and **P** configure the regional
budget, hierarchy and patch span; **Y** applies the saved profile; **T**
enables or exits online mode. The viewer runs a different physical-contact
fixture from the isolated leaderboard. A method appearing in one of the
benchmark matrices must not be described as directly compared in the GUI
unless its actual viewer control path is selected.

## Measurement boundaries

| Evidence producer | What the timer includes | What is outside the timer |
|---|---|---|
| `ccp_broadphase_leaderboard` | Query method work, construction/update and candidate-list materialization | Scene generation, oracle creation, pair-set correctness check, narrow phase and solver |
| `ccp_live_benchmark` | Full solver step with candidate-driven precise contacts and response, plus final display/min-gap scan; buckets are reported separately | Exhaustive oracle is disabled during timed paired trials; correctness is established in a prior oracle-enabled preflight |
| `ccp_multicloth_benchmark` | Full physics step and periodic exhaustive AABB oracle on every measured policy | Renderer; reference trajectory generation is separate, but reference's own step timer is also oracle-enabled |
| `ccp_online_trace` | Online and SAP step times; oracle cost is included on frames 1, 15, 30, ... | Rendering; output comparisons and CSV writes are outside simulator `step()` |
| `ccp_patch_sweep` | Complete solver steps in the later calibration/evaluation windows after warm-up | Exhaustive audit is disabled before these windows; state comparison is performed after each step |
| `ccp_cloth_stability_probe` | Not a performance benchmark; reports motion, stretch and contact stability | No CSV timing summary or speedup comparison |

The benchmark scripts generate SVG/Markdown from raw CSV; those derived files
are not independent measurements. A generated chart is only as current and
reproducible as its input CSV and recorded run environment.

## Repeat protocol for fresh evidence

Do not reuse an older CSV as the input for a new chart. For each fresh run,
create a run-specific directory and preserve raw output before summarizing.
Record the git revision, date/time, OS and CPU, compiler and build type,
Bullet version, command line, and whether the oracle was enabled. Keep each
repetition in a separate directory so that a later run cannot overwrite its
raw measurements or metadata.

Example fresh-output commands from the repository root (the full meeting
script also performs build, test, stability and supporting replay work):

```sh
mkdir -p output/evidence-runs/RUN_ID
./build/ccp_broadphase_leaderboard output/evidence-runs/RUN_ID/leaderboard.csv
python3 scripts/summarize_leaderboard.py \
  output/evidence-runs/RUN_ID/leaderboard.csv \
  output/evidence-runs/RUN_ID/leaderboard.svg \
  output/evidence-runs/RUN_ID/leaderboard.md
./build/ccp_multicloth_benchmark output/evidence-runs/RUN_ID/multicloth.csv 48
python3 scripts/summarize_multicloth.py \
  output/evidence-runs/RUN_ID/multicloth.csv \
  output/evidence-runs/RUN_ID/multicloth.svg \
  output/evidence-runs/RUN_ID/multicloth.md
./build/ccp_online_trace output/evidence-runs/RUN_ID/online-trace.csv 160
python3 scripts/summarize_online_trace.py \
  output/evidence-runs/RUN_ID/online-trace.csv \
  output/evidence-runs/RUN_ID/online-trace.svg \
  output/evidence-runs/RUN_ID/online-trace.md
```

Use distinct `RUN_ID` values for at least three repeats. Report each run's
raw CSV, then summarize the mean and spread between independent run summaries;
do not pool per-frame rows as if they were independent experiments. The
current driver uses fixed policy order, so repeated runs help expose timing
noise but do not remove order or thermal bias. For a fresh graph, regenerate
the chart from that run's CSV and keep its metadata beside the raw file.

## Verification in this audit

The updated CSV validators are exercised with valid, incomplete, duplicated,
unpaired, and malformed synthetic records by:

```sh
python3 scripts/test_evidence_summarizers.py
```

The fixture suite is intentionally small and does not launch native physics or
overwrite recorded results.
