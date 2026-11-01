# Continuously driven multi-cloth collision-candidate research

## What is actually implemented (not merely proposed)

The new scene uses the existing native OpenGL viewport, drawing LIVE particle
positions from a distinct C++23 `MultiCloth` physics class.

- **Three separate cloth sheets by default:** each is a 13 x 13 particle grid
  with 288 triangles and shared structural, diagonal and bending constraints
  solved using an XPBD-style compliant positional projection.
- **Two genuinely moving spherical obstacles:** deterministic sinusoidal
  trajectories are applied every physics step, not prerecorded animation.
- **Continuous physical excitation:** designated corner anchors oscillate,
  and the free particles experience gravity and oscillatory wind. The scene
  does not simply settle and become an uninteresting cache-hit demonstration.
- **Cloth-cloth interaction:** the experimental narrow phase responds to
  vertex-triangle and edge-edge feature proximity between DIFFERENT sheets.
  Contact corrections modify both cloths' particle positions according to
  inverse-mass weights. This demonstrates *real inter-sheet response* rather
  than kinematically intersecting mesh playback.
- **Important exclusions:** NO cloth self-contact, no time-of-impact/CCD,
  no IPC/OGC/CCFR implementation, no calibrated material realism, no strict
  intersection-free guarantee. This solver is only a physically coupled
  test environment for broad-phase research.

## Why the old guarded SAP baseline is insufficient

A global cached envelope is conservative but often returns enormous extra
candidate sets. In measured 2/3-cloth scenes, it accelerates a narrow definition
of "broad-phase lookup" but dramatically increases expensive feature testing
and full CPU step cost.

The regional method does not merely "cache a whole list":

1. Partition each cloth's **stable triangle topology** into local regions.
2. Give each region independent, validated per-primitive bounds.
3. Cache **region-pair candidate blocks**, not one monolithic global list.
4. When one region violates the geometric containment condition, rebuild only
   the pair blocks touching it; unaffected blocks stay valid.
5. **Revalidate every cached candidate against the CURRENT padded AABBs**.
   This prunes obsolete guard false positives before the precise contact tests.
6. Compare every output against independent exhaustive current padded-AABB
   enumeration and against a full SAP solution's actual particle trajectories.
7. A separate experimental variant adjusts the local guard budget using an
   exponentially weighted regional motion estimate and candidate inflation.

For currently padded primitive AABBs `B_i(t)` and cached envelopes `E_i`:

    B_i(t) subset E_i for every primitive in the certified region.

If every current required overlap appears in its cached envelope pair list,
then exact current AABB revalidation removes unnecessary candidates but does
NOT remove any required pair. When a region escapes its guard, its dependent
pair blocks are recomputed BEFORE reuse. The precise solver is separate.

This mechanism is an **experimental combination**, not established novel
prior art: AABB trees, region broad phases, dynamic fat bounds, and localized
updates already exist. Published 2026 work on solver-integrated BVH updates and
curvature-based cloth culling further narrows novelty claims.

## Measurements and reproducibility

Run `./scripts/prepare_meeting.sh`. Fresh local outputs:

- `output/native/multicloth-real.csv`: **actual physical CPU-step comparison**
  of SAP rebuild, global guarded SAP, region fixed budget, region adaptive
  budget on 1, 2 and 3 cloth sheets.
- `output/native/multicloth-real.svg`: paired full-step comparison by number
  of physically interacting sheets.
- `docs/MULTICLOTH_RESULTS.md`: measured counts, broad time, precise time,
  number of inter-sheet contact corrections, block invalidation statistics
  and max particle-state error.
- `output/native/broadphase-leaderboard.csv` / `.svg`: independent 3D
  spatial comparison against real upstream Bullet DBVT (rebuild and persistent
  update), SAP, global guard and the two regional policies.
  `simulated_cloths3` now uses **real physics-generated AABB snapshots**,
  not the older synthetic multi-sheet geometry.

The complete CPU step includes geometry/constraint work. The independent
broad-phase leaderboard times candidate search only. Do not mix these units
or claim IPC/OGC physics acceleration.

## Native controls

```sh
./open_multicloth_demo.sh
```

- K — toggle between the new multi-cloth scene and the preserved original.
- L — choose 1, 2, or 3 physically simulated cloths.
- O — choose 1 or 2 continuously moving spheres.
- Q — full SAP / global guard / region certified policy.
- A — enable exhaustive AABB candidate-coverage checks.
- G — show/hide the scrollable research inspector.
- 1 geometry, 2 certified region envelopes, 3 candidate triangles,
  4 actual inter-sheet or sphere contact triangles.
- Space pause, N single-step, R reset, F fit, W wireframe, mouse orbit/zoom.

The research pane displays live regional dirty counts, individual cached block
rebuilds/reuses, candidate revalidation rejects, actual inter-sheet contacts,
CPU stage timings, measured 4-way full solver comparison from local CSV, and
a **continuously updating candidate/contact time-series**.

## What might constitute a research contribution?

Not "we cache bounds" and not "we use local regions." These have prior art.

A meaningful hypothesis is whether a *conservative, contact-work-aware
granularity selection* policy can systematically choose region size and guard
budgets so that **saved pair-discovery work exceeds extra cache and feature
query cost**, across diverse contact formulations and real high-density
scenes, without omitting required interactions.

The present code contains a basic regional guard-budget feedback heuristic.
It does NOT yet optimize region size, prove superiority over PhysX ABP/PABP,
or integrate real IPC/OGC/CCFR. Measured fixed-vs-adaptive comparisons are
an ablation; if adaptive does not win, report that explicitly.

Prior literature worth evaluating:

- PhysX eMBP/eABP/ePABP:
  https://github.com/NVIDIA-Omniverse/PhysX/blob/main/physx/include/PxBroadPhase.h
- Bullet DBVT: https://github.com/bulletphysics/bullet3
- *Solver-Integrated Deformation-Aware BVH Updates for Real-Time Soft-Body
  Simulation*, TVCG 2026, DOI 10.1109/TVCG.2026.3729180.
- *Efficient Self-Collision Culling for Real-Time Cloth Simulation Using
  Discrete Curvature Analysis*, Mathematics 2026,
  DOI 10.3390/math14091504.

The next research task should involve a real solver's recorded geometries
or source-level adapter, multi-scale region optimization, and comparisons
with a stronger published dynamic broad-phase implementation.


## Additional hierarchical and granularity experiments

A two-level alternative performs sweep-and-prune on conservative UNION envelopes of stable mesh patches. Every patch-pair not overlapping at this coarse level is omitted from cached primitive-pair traversal; this is safe because current padded primitive bounds remain inside their enclosing patch envelopes. Newly overlapping or invalidated patches rebuild the necessary cache blocks. The original O(R squared) region-pair enumeration remains an explicit ablation. Reject duplicate primitive IDs, invalid AABBs, and non-finite coordinates.

Six fixed region sizes (1/2/3/4/6/12 cells) can be chosen without changing the solver. The patch profiler measures both all-region-pair and two-tier modes, independently verifies the same three-cloth physics as SAP, chooses using early calibration frames, and reports later-frame evaluation. No online adaptive partitioning is claimed. The regional size selection is research scaffolding to evaluate whether a more principled contact-work-aware policy is justified.

PhysX's automatic and parallel box pruning are existing multi-region alternatives, and our sweep is not claimed to be a replacement or original hierarchy design. See https://github.com/NVIDIA-Omniverse/PhysX/blob/main/physx/include/PxBroadPhase.h .
