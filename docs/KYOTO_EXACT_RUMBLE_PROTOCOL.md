# Kyoto exact broad-phase deathmatch: preregistered V5 experiment

## Why this exists

We need to distinguish an algorithm that wins because it does less exact
AABB pair work from an algorithm that appears faster because of fixed method
order, lucky seeds, or silently missing candidates. This branch preserves
the completed V4 balanced-trial evidence on
`research/kyoto-scale-evidence` and adds separately tested competing
architectures.

This is a **CPU broad-phase candidate-set** comparison, not an end-to-end
deformable dynamics, narrow-phase, GPU, or CCD comparison. No future
snapshots, collision labels, material labels, or hindsight choices enter the
online policy. Every algorithm must return the current-frame full unique
sorted set of overlapping and category-compatible AABB pairs.

## The competing architectural families

1. Rebuild-SAP and persistent coherent SAP.
2. Global guarded reuse, plus an identical separately instantiated A/A Guard.
3. Exact selective Guard and fixed/adaptive/hierarchical region methods.
4. Measured/probed Guard–SAP and bounded-trial causal controllers.
5. The original and conservative 12-feature trained MLP selectors.
6. `bvh_refit_exact`: median-built BVH with static topology and exact refits.
7. `bvh_adaptive_exact`: same BVH, causally rebuilding topology after
   excessive observed traversal work. The cost is charged to subsequent calls.
8. `grid_spatial_hash_exact`: cell-indexed exact spatial join, explicit
   handling of oversized AABBs. Excessive spills use fresh exact SAP.
9. `delta_spatial_join_exact`: incremental pair join when very few
   primitives move, exact BVH rebuild of pairs otherwise.
10. `patch_certificate_fixed`: group-enclosure certificates maintain
    conservative possible patch neighbors. Only changed patches have their
    adjacency relation repaired.
11. `patch_certificate_signal`: same *exact* certificates, but the extent
    of their margins responds to causal patch displacement, acceleration-like
    changes, and neighboring activity contrast.
12. Bullet's published persistent DBVT where the optional Bullet SDK is
    available. It should be installed for the intended Mac competition to
    include the external baseline.

The learned MLP's available actions are still the original four. Persistent
SAP and the six new policies are **not** extra output neurons of that old
trained model. A learned multi-action controller should be trained only
after the benchmark identifies useful causally observable decision features.

## Analytic kinetic deadline (separate, solver-bound component)

The standalone header `include/ccp/kinetic_certificates.hpp` implements a
conservative lower bound on earliest AABB overlap time from **guaranteed**
per-axis speed and acceleration bounds, and
`tests/kinetic_certificates_tests.cpp` verifies acceleration, immobility,
separation on multiple axes, inclusive touching and invalid inputs.
This utility is intentionally **not** entered as an extra replay method:
the current exported snapshots do not contain guaranteed future-motion
bounds. Calling previous observed velocity a guaranteed future bound
would produce a dangerously unfair and potentially incorrect shortcut.

Once a solver supplies certified motion tubes, we can benchmark truly
event-driven contact re-certification separately from the exact
snapshot-only methods.

## Safety and exactness guarantees

* The independent reference SAP computes an exact pair set before measuring
  any policy. For N<=512, it is also checked against brute-force AABB pairs
  every fifth frame; each policy's pair hash and count are compared with SAP
  on **every** frame.
* Additional adversarial regression snapshots include all-overlap, exact
  touching with zero-area boxes, teleport/scrambled order, a single moving
  object, and highly variable primitive sizes.
* The signal-guided patch controller uses frequency/activity only to choose
  a padding radius. The proof obligation remains plain geometry: each
  primitive's current bounds must fit inside the stored patch envelope
  before that cached patch adjacency can be reused. Otherwise the patch's
  potential neighbors are recomputed. There are NO uncertified future skips.
* The BVH and grid fallback to fresh exact SAP only under explicit
  worst-case work caps, with the already expended work included in time.
* The current material replay assumes stable contiguous IDs 0..N-1, as do
  several older methods; this is not yet a general arbitrary-ID API.
* This is still not continuous collision detection. Detecting an overlap
  at snapshots is different from proving that no objects crossed between
  snapshots. Verified solver-derived motion tubes are a *future* direction.

## Fairness

* Build all candidates from the same code revision using optimized Release
  C++ with the same architecture and compiler flags.
* Use the same exported, frozen, binary AABB snapshot for **every** method.
* Every policy owns its own persistent cache and pays all construction,
  refit, maintenance, reorganization, selection and sorting overhead.
* The independent reference oracle is outside measurement. Methods are
  evaluated in a seeded, reproducibly shuffled **method order** (not frame
  order), and each original snapshot is replayed several times under a
  different order.
* The duplicate Guard A/A baseline detects measurement artifacts. Results
  at or below observed A/A variation should not be called algorithmic gains.
* 16 warmup frames per replay are excluded; warmup/repetition choices are
  predeclared and retained in the immutable experiment contract.
* Identical repeats are aggregated by their median *per-scene mean* before
  comparison. They are **not** counted as independent material seeds.
* A complete experiment includes all 6 materials x 6 regimes x 3 target
  primitive budgets x 8 previously unused seeds x 3 repeated timing runs:
  **864 unique scenes / 2,592 runs**, 160 frames each (16 warmup).
* Report mean, P95, geometric mean of paired scene ratios, summed time,
  scene wins, hindsight best policies and the A/A Guard ratio. Bootstrap
  samples whole independent seeds, **not** 2,592 replay rows.
* Report the actual AABB count, which may differ from the target primitive
  budget (e.g. soft-ball meshes).
* Each new implementation is an **exploratory optimized Release baseline**,
  not a claim of exhaustive state-of-the-art parameter tuning. Fair
  comparisons cannot guarantee every family is globally optimal under all
  variants and implementation budgets.

## Isolated Mac worktree and command

From the original repository (keeps the trained model and V4 files intact):

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git fetch origin
git worktree add -b kyoto-rumble ../kyoto-exact-rumble \
  origin/research/kyoto-exact-event-field-rumble
cd ../kyoto-exact-rumble

MODEL="$HOME/Desktop/Programming/kyoto-common-contact/output/kyoto-learned-20261116T050002Z-exhaustive/model/controller.mlpc"

# Optional 12-scene smoke on a distinct output; do not confuse with
# the complete experiment. It still invokes build and oracle regressions.
KYOTO_RUMBLE_MAX_EXPERIMENTS=12 KYOTO_RUMBLE_REPEATS=1 \
  bash scripts/run_kyoto_exact_rumble_mac.sh "$MODEL" \
  output/kyoto-rumble-v5-smoke

# Full fresh-seed deathmatch; 864 unique scenes, 2592 replays:
bash scripts/run_kyoto_exact_rumble_mac.sh "$MODEL" \
  output/kyoto-rumble-v5-full
```

For an interrupted run with **the same binary hashes and parameters**:

```bash
KYOTO_RUMBLE_RESUME=1 \
  bash scripts/run_kyoto_exact_rumble_mac.sh "$MODEL" \
  output/kyoto-rumble-v5-full
```

Output includes `ABLATION_CONTRACT.json`, every snapshot, every replay CSV,
per-scene `FRESH_SEED_RESULTS.json`, timing JSON, and the automatically
computed `RUMBLE_PATTERNS.md`/`RUMBLE_PATTERNS.json` rankings and per-workload
patterns. Do not archive the huge snapshots and CSVs blindly when only a
summary upload is needed; preserve the full raw evidence locally.

## What to do with results

Look for the regions where each exact architecture beats Global Guard and
persistent SAP, and whether the same winner recurs across independent
seeds and repeated order. Test correlation with observed Guard rebuild
frequency, displacement signal, candidate density and patch repairs.

Then freeze the **winning structural mechanism** for a new independent
experiment. Do not fit a learned policy on this same held-out tournament
and call its gains independent. A future spatiotemporal contact-event
mechanism must furnish solver-derived motion bounds and a proof that no
candidate can enter between skipped updates. Compare end-to-end solver
latency and contact correctness in addition to broad-phase performance.

**No guarantee that any novel architecture will win overall.** A negative
result with exact correctness and complete work accounting is valuable.
