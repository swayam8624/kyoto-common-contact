# CBP Phase 5 — Equal-input benchmark with actual cloth topology

Status: **experimental correctness/measurement harness**, 17 November 2026. Existing V4 and material snapshot results are not altered.

## Why Phase 5 exists

On Apple Silicon, two previous `KYOTOM01` material-generator cloth-compression replay pilots yielded:

| AABB primitives | V4 mean ms | Fixed regional-exhaustive ms | V4+regional portfolio ms | Portfolio relative to V4 |
|---|---:|---:|---:|---:|
| 288 | 0.075438 | 0.078386 | 0.078435 | 0.962x |
| 512 | 0.208966 | 0.131108 | 0.150375 | 1.390x |

All seven tested methods had exact sorted candidate pair parity. The 288-object case rejected a paid regional trial and stayed in frozen V4 for 38 of 42 timed frames; the 512-object case promoted regional exhaustive and used it for 36 frames. However, V4's actual actions were `global_guarded_exact` throughout both runs (zero SAP trials), and these same seeds guided selection of regional-exhaustive as the initial portfolio trial arm. These remain **in-sample developer pilots**, not validation or novel published speedups.

The older `KYOTOM01` binary contains only AABB endpoints and a material code. It has no triangle connectivity or per-patch topology. Dividing its primitive IDs into groups of 32 is a synthetic surrogate; it does not establish the desired local contact-work hypothesis.

## What is implemented

- `apps/cbp_topology_replay.cpp` advances the existing coupled three-dimensional mass-spring `MultiCloth` solver with `Policy::RebuildSAP` to produce **one reference physical trajectory**. Each subsequent query reads `MultiCloth::currentPrimitives()`, whose triangle identities, sheet categories, masks, and patch IDs come from the actual mesh's 12x12 cells per sheet. Each moving sphere remains a separate region. Topology and vertex-sheet consistency are verified explicitly.
- It applies the SAME `kMargin/2 = 0.0375 m` broad-phase expansion as the existing multi-cloth solver, outside the timed query. It does not add a second margin. The returned categories/masks preserve self- and inter-sheet contact eligibility.
- Seven independently stateful broad-phase policies run on exactly those padded AABBs: fresh SAP reference, frozen V4, regional exhaustive, regional refit BVH, regional certified cache, Phase-3 causal whole-query manager, and Phase-4 frozen-V4 fallback portfolio. Calls rotate their external order; no competing method's duration leaks into a manager.
- Every frame's full canonical candidate list must be identical to fresh SAP; independent exhaustive pair enumeration is performed on frame 1, frame N, and periodically. Region membership is guaranteed to correspond to the mesh cell and sheet (not merely arbitrary ID blocks).
- The CSV contains actual physical snapshot metadata, patch span, number of topological regions, full per-method broad-phase query duration, exact pair count and method decisions, as well as original source solver `stepMs` **only for context**. The latter is the cost of a DIFFERENT reference policy's simulation; it is NOT a measured full-frame runtime for any new broad phase.
- `scripts/test_cbp_topology_replay.py` tests coupled one- and two-sheet trajectories with spheres and different patch spans, exact seven-method parity, immutable output, and report generation; CMake/CI uses this contract.
- `scripts/analyze_cbp_topology.py` generates descriptive timing tables and a standalone dependency-free browser dashboard with relative method performance and **observed**, not hindsight, portfolio mode timelines. It refuses to overwrite an existing HTML report.

## Intentionally absent / correctness limits

1. The source trajectory runs a single reference solver. Each broad phase is replayed, not used to integrate an independently advancing copy of the cloth; therefore this does **not** demonstrate improved cloth stability, folding, CCD, penetration, or complete solver-step time. These require a separately controlled coupled ablation.
2. A true **local multi-strategy orchestrator** is not yet implemented. The experimental portfolio selects one policy for the whole scene. The regional methods share the same top-level cross-region strategy, although they operate on topology-based patches.
3. `MultiCloth` has a 13x13-vertex cloth grid per sheet, so each sheet has 288 triangle AABBs. This is an internally controlled mesh generator, not a published real-world benchmark. Independent garment meshes, highly varying tessellation, real topology cuts, nonmanifold inputs and high-density folds must be tested.
4. This harness measures separate one-shot broad-phase calls on post-step snapshots. A correctly constructed solver integration must query intermediate solver iterations, where contact correction changes geometry. Replaying only one snapshot per timestep does not replicate the full contact solver path.
5. The portfolio's four-frame trial decision compares adjacent nonstationary frames, not the exact same frame under two cost treatments. It is a causal deployment heuristic, but not statistically unbiased. Robust new-seed validation, A/A controls, memory and migration accounting remain required.

## Reproducibility examples

```sh
cmake --preset kyoto-final-cpu
cmake --build build/kyoto-final-cpu --target ccp_cbp_topology_replay -j 4
ctest --test-dir build/kyoto-final-cpu -R cbp_mesh_topology_replay_contract --output-on-failure
mkdir -p output/cbp-phase5-local
./build/kyoto-final-cpu/ccp_cbp_topology_replay output/cbp-phase5-local/two-sheet-s1-span3.csv 2 1 48 3
./build/kyoto-final-cpu/ccp_cbp_topology_replay output/cbp-phase5-local/two-sheet-s1-span6.csv 2 1 48 6
python3 scripts/analyze_cbp_topology.py output/cbp-phase5-local/two-sheet-s1-span3.csv output/cbp-phase5-local/two-sheet-s1-span6.csv --html output/cbp-phase5-local/index.html
open output/cbp-phase5-local/index.html
```

## The next falsifiable research gate

Run matched 1/2/3-sheet scenes across patch spans 1/3/6/12, same input and Apple Silicon, checking all seven policies exactly. Report where the regional structures stop scaling and whether local patch occupancy changes the preferred algorithm. Then implement the **true per-region cost-aware routing**, with an exact cross-boundary join and a whole-scene V4 fallback, before measuring complete solver-step cost and realistic deformable datasets.
