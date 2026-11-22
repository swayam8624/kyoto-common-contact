# Stage 2 — Certified temporal refinement of a common broad phase

Date: 23 November 2026.
Status: implemented kinematic C++23 experiment and mathematical safety argument.
**NOT** a solver implementation, general CCD oracle, or published
broad-phase speedup.

## 1. The problem uncovered in Stage 1

A cubic curve x(t)=3A t(1-t) has control hull [0,A] but reaches
only [0,3A/4]. A conventional full-step moving-feature AABB containing
all four control points therefore creates a candidate for any fixed
congruent triangle with X in [0,A], even when X>3A/4 and no continuous
contact can ever occur.

One could split the entire simulation into K time slabs, construct
separate swept boxes, and run the full broad phase K times. That reduces
false positives, but may multiply bounding-box and sorting costs.

**Question:** Can we pay for tighter temporal reasoning selectively
without modifying the broad-phase semantics or dropping contacts?

## 2. Equations and one-sided correctness

Let P_i(t) be the true moving geometric primitive and let
S_i(I) be a conservative axis-aligned envelope of its positions over
time interval I. For a pair of primitives i,j, a true contact event is

    exists t in [0,1] with P_i(t) intersects P_j(t).

Using compatible masks, the single-box conservative candidate set is

    B0 = {(i,j): S_i([0,1]) intersects S_j([0,1])}.

Partition [0,1] into K synchronized intervals I_k. Define

    BK = union over k of
         {(i,j): S_i(I_k) intersects S_j(I_k)}.

In **exact arithmetic**, every true event is in BK for EVERY K:

    E subseteq BK.

Proof: choose its actual event time t*. A partition has at least one
slab containing t*. Each feature at t* lies in its own certified slab
envelope. The two envelopes intersect at the event point. Thus the
pair appears in that slab's candidate set and hence in the union.

When child enclosures are geometrically nested within their parent,
BK is also a subset of B0. Outward-rounding numerical differences can
produce wider child boxes near a boundary; the implemented selective
algorithm always starts with B0, and so returns only B0 pairs while
preserving every pair its interval tests cannot certify separated.

For cubic Bézier control points C0..C3, de Casteljau midpoint
subdivision constructs

    Q0=(C0+C1)/2, Q1=(C1+C2)/2, Q2=(C2+C3)/2
    R0=(Q0+Q1)/2, R1=(Q1+Q2)/2, S=(R0+R1)/2

Left controls:  [C0,Q0,R0,S]
Right controls: [S,R1,Q2,C3].

These exactly reparameterize the original cubic on [0,1/2] and
[1/2,1]. Repeating K=2^d times provides K synchronized subpaths.
The AABB over the interval coefficients is a conservative envelope.

The numerical implementation represents each control coordinate by
an outward-bounded interval [lo,hi], rounding EACH midpoint operation
outward using nextafter under IEEE binary64 and FE_TONEAREST.
It rejects invalid data, forbidden rounding modes and overflow.
No fast-math/FTZ/DAZ/altered FP mode is supported. This is not a
machine-certified general all-time CCD procedure.

## 3. Two independent implementations with a common query contract

### Global time-slab reference

Reparameterize and envelope ALL triangles in each slab. Use the
existing public swept-AABB primitive format with original stable IDs.
Run fixed SAP (or independent quadratic oracle) and form the unique
sorted union of interval pair sets.

### Selective pair-level refinement

First generate a B0 candidate set using the existing fixed broad
phase. Then recursively bisect only candidate pairs. At each node,
compare the two primitives' envelopes for the SAME time interval.
If they are disjoint, safely reject that branch. If both child
branches are disproved, reject the pair. If any depth-limited common
time slab overlaps, retain the pair for unchanged narrow-phase CCD.

This technique is a known style of conservative continuous collision
culling and is NOT asserted to be novel.

Both methods return a unique sorted stable-ID pair set. They
differ in paid computational work and memory scheduling. A heuristic
may choose how far to refine, but can never reject a pair merely from
estimated low probability.

### Safety boundaries

- Cross-object contact and nonlocal same-mesh self-contact need real
  mesh connectivity and filter semantics; Stage 2 still uses independent
  synthetic triangle objects.
- Any actual continuous collision has a common time; comparing
  unsynchronized subintervals is invalid as a proof of separation.
- Overlapping envelopes are only candidates; false positives remain.
- For the special congruent-triangle dataset, contact truth is
  independently calculated analytically, not from general CCD.
- Repeated timing on the same objects is NOT independent validation.
- The Stage-1 full-step hull bounds an entire cubic path only under
  its declared motion model, numeric model and verified vertex controls.

## 4. Two falsifiable geometry tests

### A. Exact mid-step contact

Moving triangle X shift is 3A t(1-t), A=4.
Fixed congruent triangle at X=3.
At t=1/2, the triangles coincide. Every refinement level retains it.

### B. Whole-path false positive (different-time aliasing)

Two congruent triangles move in X at equal velocities with separation
0.8. Their complete swept boxes overlap because their total paths
cross the same spatial region at DIFFERENT times. But their
same-time separation never changes. With sufficiently small
synchronized time slabs, the algorithm can prove they are disjoint.

Randomized validation: 96 distinct cubic triangles with independently
constructed congruent stationary shapes at randomly selected true
contact times, 5 refinement levels, independent exhaustive and fixed
SAP pair equality, plus interval point-containment sampling. Sampling
is regression evidence, not a proof of every real-valued time.

## 5. Native experiment matrix and known result

Four counts (32, 128, 512, 2,048 stationary targets) x three positive
amplitudes (2,4,8) x five subdivision levels (1,2,4,8,16 slabs)
x two methods (global vs selective) = **120 method-condition rows**.

Each row performs 3 warmups and 11 timings, paying all method-native
geometry conversion, box calculation, candidate enumeration,
refinement, sorting and normalization. The timings are one CPU
session without independent machine/thermal replication.

Representative local-container N=2,048, A=8 evidence:

| Method | Slabs | Unique candidates | Analytic true events | False positives | Mean full query ms |
|---|---:|---:|---:|---:|---:|
| Global swept-box SAP | 1 | 1,707 | 1,280 | 427 | ~0.327 |
| Global all-object subdivision | 16 | 1,280 | 1,280 | 0 | ~72.24 |
| Selective pair refinement | 16 | 1,280 | 1,280 | 0 | ~24.00 |

The selective method is ~3x faster than **global high-depth
refinement**, NOT faster than the simple original coarse query.
This is a real measured negative result for end-to-end broad-phase
query acceleration on this synthetic workload. It is not externally
validated, not a solver result, and cannot imply benefit from
avoided narrow-phase costs until a real narrow phase is measured.

No user-specified threshold such as 1.2x against official LBVH has
been achieved.

## 6. Visual deliverables and their source

The C++ CLI exports:

- temporal_analytical_study.csv: all 120 measured rows
- candidate_graph.csv: 160 native target/edge classifications
- slab_envelopes.csv: 31 native de Casteljau boxes
- manifest.json: exact source and limitation declaration

The Python renderer consumes only those files to generate:

01. false-positive elimination stacked bars
02. paid time versus subdivision depth (log y)
03. 3D measured CPU cost mesh, markers at every measured point
04. global/selective regime heatmap
05. 3D abstract contact candidate graph (only node X is spatial)
06. 3D certified spacetime-envelope layers
07. offline interactive 3D observatory plus combined atlas

Graph Y/Z positions for the contact network are abstract layout
coordinates, not physically simulated positions. Likewise, the
certified envelope layer plot is a kinematic representation, not
a stress/pressure map.

All original negative outcomes and failure cases remain visible.

## 7. Competing official IPC Toolkit: interface review

Checked the official IPC Toolkit source (2026-11-23).
Current latest stable GitHub release is **v1.6.1**, October 22, 2026.
Pinned release tag commit:
19bcab7e1f5a0d2addfcd28513422ff248ea504e.

Development main, inspected separately on 2026-11-23:
4e16ebc943d0d1a3206e6f6af93a6001218bf9a4.
DO NOT silently treat main APIs as stable-release APIs.

The official header has ipc::LBVH, derived from ipc::BroadPhase.
The base supports static vertex positions, linear endpoint motion,
and construction from explicit vertex AABBs. The API enumerates
specific collision feature types: edge-edge, face-vertex etc,
with its own vertex adjacency and mask filtering. These are NOT
equivalent to our triangle-triangle AABB pair interface.

**Required next adapter**:

1. Build a shared-vertex mesh connectivity contract: V, E, F with
   stable IDs and one certified swept envelope per original vertex.
2. Construct official ipc::AABBs for vertex boxes in matched units,
   radius conventions and motion assumptions; build official LBVH.
3. Compare equivalent face-vertex/edge-edge candidate semantics,
   not triangle-triangle vs IPC's arbitrary primitive counts.
4. Run official independent CCD narrow-phase for actual event truth.
5. Explicitly account for all extraction, conversion, LBVH,
   candidate materialization and collision-search costs.
6. Repeat on a real solver (PolyFEM or another externally validated
   deformable solver) plus an independent second integration.

IPC Toolkit does not supply a complete physical simulation itself;
label every kinematic test accordingly.

## 8. Critical future design choice: rent versus avoidance

A selective strategy should refine a pair only if its expected
avoided narrow-phase work exceeds its measured refinement expense:

    E[p_narrow * cost_narrow_avoided] >
       cost_refine + added_index_cost + added_allocations.

p_narrow cannot become a correctness filter. A safe controller may
choose refinement depth (including zero) to lower total measured work,
while all coarse candidates go to the true narrow phase if no safe
exclusion proof exists. Cost calibration must come from TRAINING
scenes, not from the untouched held-out confirmation set.

Next gates: official feature-based IPC adapter, real solver contact
events, selective depth controller, end-to-end measured benefit,
and scientific visualization from actual solver fields rather
than synthetic analytic shapes.

## 9. Sources

- https://github.com/ipc-sim/ipc-toolkit/releases/tag/v1.6.1
- https://github.com/ipc-sim/ipc-toolkit/blob/v1.6.1/src/ipc/broad_phase/broad_phase.hpp
- https://github.com/ipc-sim/ipc-toolkit/blob/v1.6.1/src/ipc/broad_phase/lbvh.hpp
- https://github.com/ipc-sim/ipc-toolkit/blob/v1.6.1/src/ipc/broad_phase/aabb.hpp
- https://github.com/ipc-sim/ipc-toolkit
