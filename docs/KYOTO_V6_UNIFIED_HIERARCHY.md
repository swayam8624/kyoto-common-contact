# Kyoto V6 — Unified Certified Interaction Hierarchy (experimental, exact AABB snapshots)

## Why V5 changes the research problem

The 864 unique material × regime × target-size × seed cells with three
method-order repetitions each establish **multiple structural regimes**.
The bounded Guard/SAP trial achieved 1.0912× scene-geometric and 1.3287×
summed-scene-time speedup relative to Global Guard, winning 667/864 cells.
Its duplicate Guard A/A control was 1.00046× overall. The original MLP
selected Global Guard on ~89% of frames.

But the standalone methods provide complementary evidence: fixed patch
interaction certificates beat bounded trial by 2.09× geometrically for
spring target-128 and 1.39× for spring target-512 (48/48 cells favor patches
by over 5% in each). Large cloth favors adaptive BVH over bounded by ~1.52×
geometrically, and large soft-ball scenes similarly favor a refit BVH by
~1.69×. Adaptive BVH's global typical-scene ratio is only 0.590× versus
Guard, even though its global summed-time ratio is 1.473×—it loses many
cheap scenes and wins expensive ones. A fixed signal-padding heuristic adds
only ~0.6% geometric advantage to fixed patch overall and is not
automatically worth its computational complexity.

These are **posthoc whole-trajectory observations** from generated workloads,
not a deployable oracle. A merged algorithm must pay its own transition,
maintenance and index costs on every frame. The independent V5 JSON evidence
does not include raw replay CSVs; full source-data hashes and frame-by-frame
pair hashes cannot be independently re-audited from the shared summaries
alone.

## Design objective

A collision accelerator should maintain ONE persistent object representing
what spatial interactions remain possible. It may use different *work
strategies* for that same index, without running independent Guard and SAP
algorithms every frame.

For each AABB `$B_i(t)# Kyoto V6 — Unified Certified Interaction Hierarchy (experimental, exact AABB snapshots)

## Why V5 changes the research problem

The 864 unique material × regime × target-size × seed cells with three
method-order repetitions each establish **multiple structural regimes**.
The bounded Guard/SAP trial achieved 1.0912× scene-geometric and 1.3287×
summed-scene-time speedup relative to Global Guard, winning 667/864 cells.
Its duplicate Guard A/A control was 1.00046× overall. The original MLP
selected Global Guard on ~89% of frames.

But the standalone methods provide complementary evidence: fixed patch
interaction certificates beat bounded trial by 2.09× geometrically for
spring target-128 and 1.39× for spring target-512 (48/48 cells favor patches
by over 5% in each). Large cloth favors adaptive BVH over bounded by ~1.52×
geometrically, and large soft-ball scenes similarly favor a refit BVH by
~1.69×. Adaptive BVH's global typical-scene ratio is only 0.590× versus
Guard, even though its global summed-time ratio is 1.473×—it loses many
cheap scenes and wins expensive ones. A fixed signal-padding heuristic adds
only ~0.6% geometric advantage to fixed patch overall and is not
automatically worth its computational complexity.

These are **posthoc whole-trajectory observations** from generated workloads,
not a deployable oracle. A merged algorithm must pay its own transition,
maintenance and index costs on every frame. The independent V5 JSON evidence
does not include raw replay CSVs; full source-data hashes and frame-by-frame
pair hashes cannot be independently re-audited from the shared summaries
alone.

## Design objective

A collision accelerator should maintain ONE persistent object representing
what spatial interactions remain possible. It may use different *work
strategies* for that same index, without running independent Guard and SAP
algorithms every frame.

, a maintained loose envelope `$E_i# Kyoto V6 — Unified Certified Interaction Hierarchy (experimental, exact AABB snapshots)

## Why V5 changes the research problem

The 864 unique material × regime × target-size × seed cells with three
method-order repetitions each establish **multiple structural regimes**.
The bounded Guard/SAP trial achieved 1.0912× scene-geometric and 1.3287×
summed-scene-time speedup relative to Global Guard, winning 667/864 cells.
Its duplicate Guard A/A control was 1.00046× overall. The original MLP
selected Global Guard on ~89% of frames.

But the standalone methods provide complementary evidence: fixed patch
interaction certificates beat bounded trial by 2.09× geometrically for
spring target-128 and 1.39× for spring target-512 (48/48 cells favor patches
by over 5% in each). Large cloth favors adaptive BVH over bounded by ~1.52×
geometrically, and large soft-ball scenes similarly favor a refit BVH by
~1.69×. Adaptive BVH's global typical-scene ratio is only 0.590× versus
Guard, even though its global summed-time ratio is 1.473×—it loses many
cheap scenes and wins expensive ones. A fixed signal-padding heuristic adds
only ~0.6% geometric advantage to fixed patch overall and is not
automatically worth its computational complexity.

These are **posthoc whole-trajectory observations** from generated workloads,
not a deployable oracle. A merged algorithm must pay its own transition,
maintenance and index costs on every frame. The independent V5 JSON evidence
does not include raw replay CSVs; full source-data hashes and frame-by-frame
pair hashes cannot be independently re-audited from the shared summaries
alone.

## Design objective

A collision accelerator should maintain ONE persistent object representing
what spatial interactions remain possible. It may use different *work
strategies* for that same index, without running independent Guard and SAP
algorithms every frame.

 is a
current-time **containment certificate**:

$
B_i(t) \subseteq E_i(t).
$

For every compatible pair whose envelopes intersect, store an edge in a
potential-contact graph. Actual query results are the graph edges that
overlap as current AABBs. Thus any current exact overlap is present because

$
B_i \cap B_j \neq \varnothing \Longrightarrow E_i \cap E_j \neq \varnothing.
$

If an AABB exits its envelope, the affected potential graph edges must be
repaired **before** filtering and returning exact candidate pairs.

This preserves the same current-AABB candidate-pair semantics as the V5
oracles. It does NOT assert safety between discretely sampled frames.

## Implemented V6 prototype — `unified_certified_hierarchy_v0`

Source: `include/ccp/unified_certified_hierarchy.hpp`.

* One persistent adaptive/refit BVH indexes either cached loose envelopes
  or exact current AABBs. This is not a wrapper that runs the existing
  GlobalGuard or maintains a separate persistent SAP cache. The underlying
  exact BVH implementation nevertheless has a worst-case **fresh SAP
  fallback** when traversal work exceeds its explicit cap; that fallback
  and the aborted BVH work are both charged to runtime.
* In **certified reuse** mode, all primitives undergo containment and
  collision-filter-signature checks every frame. If all pass, reuse
  the complete potential-pair list and perform only current exact AABB
  intersection checks.
* If only a few envelopes escape, remove candidate graph edges incident
  on dirty primitive IDs and recompute every such primitive's possible
  interactions. This is exact incremental-join maintenance; no heuristic
  may delete a necessary edge.
* If a moderate number of envelopes escape, rebuild the potential-pair
  graph from the refitted BVH using updated loose envelopes.
* After sustained high invalidation activity on larger scenes, traverse
  that **same BVH** over raw current bounds rather than maintain an
  oversized potential graph. Return to certified reuse only after a
  sustained lower-motion window.
* A low-cost neighboring displacement-difference signal adjusts padding,
  **not whether containment is checked**. This is an ID-neighbor proxy,
  not a mesh-topology wavelet and not a future-motion guarantee.
* No labels, future replay frames, hindsight winners, neural inference or
  independent backend timing enter mode decisions.

The prototype is intentionally conservative; it has NOT been shown to
outperform V4, V5 methods, any published production engine, or the strongest
posthoc baseline. Branch isolation preserves V5.

## Correctness argument

1. Initialization builds all containment envelopes and uses an exact
   BVH traversal to enumerate compatible overlaps of those envelopes.
2. If none of the signatures changes and no current AABB exits its stored
   envelope, the potential graph remains a superset of current exact
   overlaps; exact filtering produces precisely the current candidates.
3. If a subset of envelopes is dirty, all previously cached edges
   incident on those IDs are removed, and all updated envelope pairs
   incident on those IDs are enumerated. Edges between untouched
   envelopes remain valid because neither endpoint's signature nor
   envelope changes. Exact filtering is therefore still complete.
4. When most envelopes require refreshing, a full exact refit BVH traversal
   regenerates the potential graph.
5. Direct mode refits the same BVH on actual current AABBs and emits exact
   pairs. No speed threshold or activity estimate can cause missing pairs.

The current replay assumes stable ordered IDs 0..N-1. Arbitrary IDs,
topological changes, contact masks that change without supplied AABBs,
and continuous-time crossing need their own API and tests.

## The full planned mechanism is *larger* than V6 v0

A credible culminating architecture should replace ID-contiguous patches
with geometry/topology-local clusters; split/merge spatial clusters
using measured incident-edge repairs and refit cost; use solver-provided
**certified** motion bounds to schedule event-driven re-certification;
retain a single index across transitions; and, only when it outperforms
cheap deterministic rules, use an ML policy for cost/horizon estimation.
ML outputs must never authorize skipping required proof checks.

The V6 proof-of-concept currently tests only the core exact representation
and the two maintenance strategies (reuse/repair and direct hierarchy).
There is **no claim** that topology-aware patch splitting or guaranteed
future-time kinetic scheduling has been implemented.

## Fresh-seed evaluation contract

Do not use inspected V5 seeds 3203/3217/3251/3299/3313/3343/3371/3407
as an independent confirmation of V6. The new runner
`scripts/run_kyoto_unified_v6_mac.sh` uses 8 untouched seeds
4103/4111/4127/4153/4177/4201/4217/4231.

1. Compile with Release optimizations, Bullet DBVT installed, and the
   `KYOTO_REPLAY_ABLATION_V6` capability signature.
2. Run the exact-pair oracle, five adversarial scenarios, small learned
   regression tests and the four-CSV end-to-end report test.
3. Test 36 balanced new-seed scenes first, two randomized-order repeats
   each, with 160 frames and 16 warmup frames. This is development, not an
   exhaustive claim.
4. Inspect per-frame dirty counts, candidate-pair list size, mode
   transitions, total query mean and P95, memory and A/A control.
5. Only after correctness and credible speed is observed, freeze the
   implementation and run all 864 newly seeded cells × 3 repeats in an
   uninspected output directory. Report failures as failures.
6. Integrate into the full coupled cloth/soft-body solver and compare
   narrow-phase contacts, collision correctness and end-to-end step time.

The **final mechanism** is not ready to name a winner. This file is an
inspectable, reproducible engineering hypothesis.

## Scientific prior art to compare

* Kinetic Separation Lists for Continuous Collision Detection of
  Deformable Objects (Weller and Zachmann, 2006).
* Existing kinetic data structure separation certificates and event
  queue maintenance.
* Deformation-aware BVH refit and solver-integrated updates
  (including 2026 IEEE TVCG research).
* IPC Toolkit's LBVH and spatial-hash families and 2025–2026 GCP/
  memory-optimized broad-phase updates.
* Optimized real-world dynamic BVH, SAP, and region-local collision
  implementations, with independent tuning and equal correctness
  contracts.

Novelty is not established merely by combining these known ideas.
