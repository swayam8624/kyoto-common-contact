# Kyoto Common Broad-Phase — falsifiable research charter (17 Nov 2026)

**Status: experiment, not a publication or a novelty claim.**

## Problem and distinction

The V4 Guard/SAP bounded-trial controller repeatedly beat Global Guard on certain workloads, especially large cloth compression and spring compression. V5's 20-method tournament exposed *complementary* wins from fixed patch certificates and refit/adaptive BVHs. V6 attempted an integrated hierarchy and lost overall, especially because a small dirty set still triggered an O(dirty × N) incident-edge scan and large potential-pair filtering.

An **offline per-scene best method** is not an online algorithm: deciding which backend wins, warming or migrating its state, maintaining dormant caches, and comparing alternatives all have nonzero cost. Likewise, **scene-level wins do not imply region-level wins**. The hypothesis below must be falsified on actual region-sized workloads and full simulation trajectories.

**Hypothesis:** An exact, single-owner spatial interaction index with topology-aware local regions, cost-budgeted certificate maintenance, indexed dirty-edge repair, and conservative direct fallback can outperform a strong fixed global method on heterogeneous, spatially nonstationary *deformable* scenes **including all control/repair/synchronization costs**, while maintaining identical current-AABB candidates and identical physical trajectories. This is distinct from running every candidate method or keeping several complete global structures hot. An efficient online manager is the *mechanism to test*, not itself established novelty.

### Research questions

1. **Crossover:** Are V5's per-material/sweep crossover observations reproduced when measured for local **region-owned** work and border interactions, rather than full independent trajectories?
2. **Cost accounting:** Is the index repair + local routing cheaper than exact global Guard/SAP/BVH, once cross-border joins, candidate deduplication, state migration, pair-list materialization, features, and memory are all charged?
3. **Completeness:** Can we guarantee all compatible current-AABB overlaps across changing region ownership without falling back to O(dirty × N) maintenance or relying on stale future predictions?
4. **Solver:** Does the contribution improve full CPU deformable timestep and contact correctness, not just standalone AABB replay timing? What happens under dense self-contact and worst-case adversarial motion?
5. **Generality:** Can the benefit survive independent external deformable sequences, new seeds and machines, and a fair comparison with contemporary IPC Toolkit LBVH and solver-integrated BVH methods?

## An uncomfortable upper-bound observation

The V5 post-hoc per-WHOLE-scene 4-policy oracle (Guard, V4, adaptive BVH, fixed patch) reported 1.3034x geometric speedup vs Guard; V4 was 1.0912x. Thus choosing the best of those four methods with perfect hindsight would offer only roughly 1.19x *additional geometric speedup over V4*, **before** paying prediction, cache migration or selection overhead. Its summed-scene-time speedup was 1.9408x vs Guard, so costly scenes offer larger time-weighted opportunity. These are not hard bounds on a NEW local-index mechanism: a system that exploits spatially heterogeneous subregions might exceed the global per-scene oracle, but that must be measured, not assumed.

A manager that merely recognizes material names or predicts the best entire method from held-out full-scene labels is NOT a sufficient novelty hypothesis. Material labels represent procedural synthetic AABB generators; we must show the causal geometric/maintenance signal generalizes to physical mesh trajectories and genuine external datasets.

## Existing contributions versus proposed contributions

| Stage | What is actually implemented | Scientific claim |
|---|---|---|
| V4, frozen | Guard/SAP bounded trial | Repeated CPU standalone wins vs Guard, not universal or TOG novelty |
| V5, archived | 20-method exact candidate benchmark | Structural workload crossover; not a deployable oracle |
| V6, archived | single-index certified hierarchy | Negative result; diagnose maintenance bottleneck |
| CBP Phase 0, THIS BRANCH | exact regional router with independent cross-region exhaustive oracle, local exhaustive or coherent-SAP kernels, telemetry, dynamic repartition tests | **No speed or novelty claim**; foundation to falsify local delegation |
| CBP Phase 1, IMPLEMENTED (experimental) | one shared refittable BVH over current primitive bounds; cross-region dual-tree traversal, local SAP/exhaustive region kernels and changed-leaf/ancestor refits | Exactness CI passed; no overall performance or generality claim |
| CBP Phase 2, NOT IMPLEMENTED | conservative per-object/patch containment certificates and indexed dirty-ID incident-edge repair using the Phase-1 BVH | Remove V6's O(dirty × N) scan, establish exactness and measure maintenance costs |
| CBP Phase 3, NOT IMPLEMENTED | measured-cost, switching-penalized local maintenance decisions; cheap escape to direct traversal | Show a real *deployed* advantage over fixed best; no hindsight leakage |
| CBP Phase 4, NOT IMPLEMENTED | solver-integrated dirty propagation and validated continuous swept-bounds support | End-to-end demonstration and CCD-appropriate contract |

## Correctness contract: discrete frame t

Let B_i(t) be a finite AABB with stable ID, category/mask metadata, and valid endpoints. The output must be exactly the unique sorted set

    C(t) = { (i,j) : i<j, compatible(i,j), B_i(t) intersects B_j(t) inclusively }.

For a partition into regions R_k, every same-region pair must be checked in its local kernel. Every cross-region pair must be considered when the current AABBs overlap. Phase 0 prunes conservative region unions, then uses an O(|R_a| |R_b|) cross join. Phase 1 instead stores current AABBs in one shared BVH. Every internal node bounds its descendants, so a nonoverlapping node pair cannot contain a crossing primitive pair. Dual-tree traversal prunes such node pairs; uniform-region subtrees are skipped because the local kernels handle them. Every changed geometry leaf marks ancestors dirty and refits them in child-before-parent order before enumeration. Ownership remapping rebuilds the index. Exact leaf filter checks maintain the canonical output contract.

For future certificate reuse, maintain B_i(t) subset E_i(t), and potential graph P containing every compatible pair whose certified covers E_i and E_j overlap. Dirty IDs must repair all incident graph edges before filtering current candidates. If labels, topology, signatures, cover containment, or solver motion bounds invalidate any invariant, synchronously rebuild the affected structure or take an exact fallback. No heuristic or ML inference may bypass validation. This is **not** proof of continuous collision safety; CCD requires conservative swept spacetime bounds and a separate narrow-phase.

## Timing contract and benchmark ethics

Charge **all** costs at the actual call boundary: bound extraction, dirty tracking, region assignment, local query, shared-index update/refit, cross-region candidate traversal, membership migration, duplicate normalization, policy selection, and memory allocation. In full physics, also separate narrow phase, solver and diagnostic/oracle cost; disable diagnostic overhead in unbiased timings while keeping separate exactness runs.

Use warm-up and source/seed freezing; randomized policy order; repeated independent cohorts; A/A controls; full per-frame pair counts and hashes against SAP plus independent brute-force at manageable N; adversarial topology/mask/geometry changes; failure and timeout accounting. Report geometric-mean and summed-time scene speedup, P95, wins/losses by scene and fixed subgroup, peak RSS, total step latency and correctness. Never count repeated same-seed runs as independent data. The V5 and V6 seeds are **exploratory/inspected**, not untouched confirmation.

Compare against unchanged V4, V5 patch/BVH family, Guard, persistent SAP, a duplicate Guard A/A, a static best method chosen on *training* data, a **hindsight oracle labelled unattainable**, and a source-referenced IPC Toolkit LBVH with matched broad-phase candidate semantics. A paper-quality result needs a positive advantage over the best reasonable *deployable* baseline, not only Global Guard.

### Pre-registered engineering success targets (aspirational, not findings)

- Exact pair parity 100% on the declared discrete-AABB contract; independently tested on all smaller cases, no skipped failures.
- Zero stale pair / mismatched region ownership bugs under 90+ dynamic frames and adversarial region remapping, filtering, degenerate/touching boxes, sudden movement.
- Target >=1.20x geometric query-speedup versus strongest fixed deployable baseline on a heterogeneous **new** test cohort, with bootstrap interval excluding 1x, after selector and migration overhead; no uncontrolled >2x tail regression.
- Target >=1.10x complete solver-step speedup on real deformable solver scenes *if broad-phase occupancy permits*; no change to correct narrow-phase contact semantics or positional trajectories beyond numerical tolerance.
- Surface a detailed ablation: no cost controller, no local repairs, global-only modes, fixed vs adaptive region ownership, all-cache-hot (for diagnostic only), real cold/migration timing, and full index maintenance.

Failure to reach these thresholds means the hypothesis needs revision, not more post-hoc seeds or a changed plot scale.

## Strong prior art to beat or differentiate

- Avril, Gouranton and Arnaldi (2011), Dynamic Adaptation of Broad Phase Collision Detection Algorithms — direct adaptive-manager prior art; distinguish this explicitly before claiming a novel selector.\n- Weller and Zachmann (2006), Kinetic Separation Lists for CCD of Deformable Objects — event-based hierarchical temporal coherence. https://doi.org/10.2312/pe/vriphys/vriphys06/033-042
- Adaptive Collision Culling for Massive Simulations by a Parallel and Context-Aware Sweep and Prune Algorithm — adaptive partitioning is known. https://pubmed.ncbi.nlm.nih.gov/28574360/
- Flexible Use of Temporal and Spatial Reasoning for Fast and Scalable CPU Broad-Phase Collision Detection Using KD-Trees — hybrid spatial approaches are known. https://onlinelibrary.wiley.com/doi/10.1111/cgf.13529
- IPC Toolkit official LBVH — published, optimized production-style external baseline. https://github.com/ipc-sim/ipc-toolkit/releases
- Solver-Integrated Deformation-Aware BVH Updates for Real-Time Soft-Body Simulation (2026) — particularly close solver-integrated dirty-propagation prior art. https://pubmed.ncbi.nlm.nih.gov/42672017/
- Dynamic Clustering for topology-changing CCD — clustering is not novel by itself. https://pmc.ncbi.nlm.nih.gov/articles/PMC4500942/

**Novelty review gate:** Before claiming a contribution, create a claim-by-claim matrix identifying what none of these papers does, with exact equations, datasets, measured costs, formal statements and code diffs. Marketing terms such as “adaptive”, “certified”, “hybrid”, “manager”, or “ML” are not arguments for novelty.

## Graphics-grade demonstration contract

A synchronized, looping grid shows high-N cloth compression and rapid motion, spring compression and shear, local self-contact, two/three-cloth collisions, multiple spheres, articulated/soft objects, abrupt regime transitions, quiet-motion caches, and concentrated global contact. Every animation panel is a *real* trajectory from the same deterministic solver conditions; exact algorithms should produce the SAME geometry. Per-method overlays show frame time, broad-phase time, candidate count, narrow-phase work, real contact events, dirty regions, cache bytes, cross-region query count, maintenance mode, and switch cost. A separate instrumented representation visualization shows certificate envelopes, graph edges, updated leaves/ancestors and policy decisions; a dramatic difference in cloth folds is **not** claimed if the candidate sets and solver are identical.

**Never reuse synthetic showcase videos as proof of real-world physical realism.** For physics quality, add actual post-solve triangle-triangle self-intersection checks, continuous tunneling cases, and independently aligned external ground truth when available.
## Phase 1 implementation ledger (experimental, 17 Nov 2026)

- `include/ccp/cbp_spatial_index.hpp`: a shared median-split primitive BVH, current-frame exact AABBs, incremental leaf/ancestor refits, dual-tree enumeration of cross-region candidate pairs. Rebuild on label changes. Metadata filtering remains at the primitive leaf so mask/category-only changes are respected.
- `include/ccp/cbp_region_router.hpp` now defaults to the shared indexed cross join but retains `CrossMode::Exhaustive` as a measured A/B control. Same-region kernels retain exhaustive/coherent SAP; the combined output stays sorted and unique.
- `tests/cbp_shared_bvh_tests.cpp` checks against an independent all-pairs oracle through remapping, masks, sudden motion, touching, zero-width boxes, empty input and 140 dynamic frames, and verifies pruning in a deliberately sparse interleaved-region scene.
- Existing Phase-0 test uses explicit exceptions in place of release-disabled `assert`, so validation remains meaningful under `-DNDEBUG`.
- `apps/cbp_cross_microbench.cpp` compares entire router call times on spaced, clustered and dense synthetic regimes with alternating method order and independent persistent caches. Includes mean/P95 and actual primitive-test and refit counters. It is diagnostic, not a confidence interval or an external dataset result.

**Limitations:** The BVH still scans input bounds each frame, rebuilds on any region reassignment, and traverses cross-region pairs each query. It does not cache per-patch certified potential-contact edges, implement an adaptive runtime cost manager, or guarantee continuous collision safety. Region-union calculation and final pair sorting remain additional overhead. The tree can deteriorate spatially under sustained large deformation.

**Phase 1 acceptance gate:** Optimized `-Werror`, sanitizer tests, and exact C++ oracles must pass. After that, measure the new path against Phase 0 and against V4 on identical replay inputs, including all maintenance and normalization costs; report regressions as regressions.
