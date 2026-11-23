# Bounded temporal frontier: invariants, correctness scope and research hypotheses

> This is an **implementation-level proof obligation** for the actual
> `RefitHierarchy` C++23 code, not a claim of formally verified
> floating-point collision detection, physical correctness or novelty.
> Inspectable implementation: `include/kyoto/refit_feature_hierarchy.hpp`.
> Reference semantics: `include/kyoto/feature_broadphase.hpp`.

## 1. Domain and benchmark contract

A solver source step supplies a finite indexed mesh
`M=(V,E,F)`, complete cubic Bezier motion control points for each
vertex, an outward skin `δ >= 0`, filter categories/masks and
an explicitly identified topology epoch. The output is a set of
**potentially colliding feature pairs**:
vertex–face (VF) or distinct edge–edge (EE). Incident features
and forbidden category/mask pairs are excluded.

The *reference* set `C(M)` is obtained by enumerating every
admissible VF and unordered EE stencil, then checking its
complete swept AABB intersection. This is deliberately *not* a
set of continuous narrowphase collision events. Such events
require a separate CCD predicate and source motion contract.

The production code uses three BVHs: one over vertex boxes, one
over face boxes, and a self-tree over edge boxes. A node bounds the
union of every feature record in its subtree.

## 2. Geometry lemma: conservative swept bounds

A cubic Bezier vertex path is

`x(t) = Σ_(i=0)^3 binom(3,i)(1-t)^(3-i)t^i P_i`,
for `0 <= t <= 1`.

In exact arithmetic, its Bernstein weights are nonnegative
and sum to 1. Thus every coordinate lies between the smallest
and largest matching control-point coordinates. Inflating
those axis bounds by the collision skin `δ` conservatively
contains the complete vertex trajectory. Unions of incident
vertex envelopes conservatively contain a moving edge or
moving triangle. Unions of child boxes conservatively contain
every descendant of an interior tree node.

**Floating-point boundary:** Source `vertex_box()` applies
`std::nextafter` outward to its inflated bounds. This improves
conservativeness but is *not* a complete machine-directed-rounded
interval arithmetic proof across arbitrary compilers, overflow,
underflow, FMA modes or adversarial values. The exact-arithmetic
containment argument and the finite IEEE tested implementation
must not be conflated.

## 3. VF node-product partition invariant

Write `L(N)` for a BVH node's descendant leaves.
The complete VF search space is the Cartesian product
`L(V_root) × L(F_root)`.

A persisted frontier entry `(a,b)` represents **the entire**
product `L(a) × L(b)`. At every physical source frame, the stored
entries form pairwise disjoint products whose union equals the
complete VF search space. Initialization has just the root pair.

Given a stored `(a,b)`, the bounded traversal either:

1. Finds its two node bounds disjoint, keeps the **same entire
   product** as one non-overlap certificate for this frame, and
   emits no candidate.
2. Finds two leaves overlapping, checks the exact feature-level
   incident and mask policy and emits the candidate if valid.
3. Splits an interior node into children and replaces the parent
   by all the resulting products, preserving a disjoint partition.
4. Has insufficient frontier budget to split. It retains the
   **coarse parent product** and runs ordinary complete dual-tree
   traversal over that parent for the current frame.

The key safety point is (4): **retained does not mean ignored**.
A coarse overlapping frontier product always receives full paid
enumeration for this frame. Previous-frame separation is **never**
carried into a new frame without rechecking the refitted boxes.

By induction on the number of split/retain operations, the
frontier continues to partition the complete VF product and
emits exactly the same admissible overlapping leaf pairs as
the exhaustive reference under the exact-arithmetic box assumptions.

## 4. EE unordered self-tree partition

The EE domain is `{(e_i,e_j) : i<j}` for distinct edge leaves.

For a diagonal internal node `(a,a)`, left/right children give
three disjoint subdomains:
`(a_L,a_L)`, `(a_L,a_R)` and `(a_R,a_R)`.
A diagonal leaf contributes no distinct edge pair.
An off-diagonal product splits whichever endpoint is not a
leaf into two disjoint products. A separated product is
retained as a current-frame rejection; a budget-limited product
is retained as a coarse cell and fully scanned with the
ordinary unordered self-tree method.

This exactly partitions unordered distinct EE leaves and avoids
double counting. Edge IDs are canonicalized before candidate
normalization. Shared-vertex incidence and filter masks are
checked at leaves.

## 5. Why the frontier memory cap is **preemptive**

For either frontier family let `S` be the size of the pending
work stack and `N` the number of finalized retained entries.
The implementation initializes work with at most one root
(or a previously retained frontier already below the cap `B`).

Before expanding a popped node pair into `k` children, it checks

`N + S + k <= B`.

If this is false it finalizes the original pair as a coarse
frontier entry and fully traverses its product without
enqueuing any children. Otherwise it enqueues all `k` children.
As a consequence the frontier cannot first grow unbounded and
only afterward be thrown away. Each of the VF/EE frontiers
receives a separately budgeted share of the total limit
`B = max(64, 4(|V|+|E|+|F|))`.

**Runtime caution:** A coarse product can still cause a costly
`O(|V||F| + |E|²)` worst-case candidate enumeration. The
budget bounds retained **frontier memory**, not candidate count
or worst-case time. That explicit fallback is preferable to
silently missing a collision candidate.

## 6. Full cache invalidation and genuine motion

The cache signature contains source epoch, record counts,
record IDs, object identities, connectivity, categories,
masks and their order. A change causes a full hierarchy
topology rebuild and resets the old pair cut. A motion-only
change causes full swept-bound recomputation, a bottom-up tree
refit and a recheck of every retained node pair. Thickness
changes also recompute every box, even without epoch change.

Current source tests cover abrupt changes in cubic controls,
high relative displacement, coalescing/separating clusters,
mask changes, topology/feature deletion, source skin changes,
NaN rejection/recovery, and density-driven budget fallback.

A cached source record is not a physical collision:
`RefitHierarchy::query()` never changes FEM forces, positions,
impulses, or the solver's accepted physical duration.

## 7. Empirical evidence without extrapolation

- The independent exhaustive reference checks typed VF/EE
  *candidate coverage*, not exact CCD events.
- Reproducible source levels include Pass 11 original real
  neo-Hookean 3D FEM, Pass 13 original physical snapshots,
  Pass 14 BVH parity, Pass 17 dense/isolated cubic-motion
  stress, and Pass 19 1120 seeded feature-level fuzz frames.
- Actual accepted 20 m/s fast-impact FEM leaves total 0.75 ms.
  The official IPC CCD audit of three accepted physical leaves
  found **zero actual cross-object CCD events**; that is expected
  for an accepted nonintersecting step and cannot serve as a
  positive detection control.
- An additional separately labeled *solver-rejected* original
  full-dt proposal is being audited with actual pinned IPC
  narrowphase to require **nonzero** real CCD events. It is never
  labeled accepted physical integration.
- Paid native measurements distinguish independent exhaustive,
  rebuilt SAP, persistent-order SAP, cached BVH, uncached BVH,
  bounded pair frontier, and official pinned IPC LBVH. The
  stronger cached-input IPC adapter is an explicit fairness
  control: benchmark advantages before its validation are not
  strong evidence of superior data structures.

The above regression cases are **finite tests**. CI passing
does not transform them into formal proofs of all IEEE
floating-point inputs, every topology, or all physical solvers.

## 8. Novelty and falsification

Incremental BVH front tracking and persistent contact frontiers
appear in prior deformable-collision research. The simple
retention idea must **not** be claimed as a new invention.

A potentially valuable *research hypothesis* is that typed,
budgeted, correctness-preserving frontier maintenance improves
total charged cost on particular real deforming scenes, even
against optimized pinned upstream baseline preprocessing.

Tests capable of refuting this claim include:
- an equally cached official IPC LBVH input converter;
- full successive source steps rather than sparse snapshots;
- independent mesh resolutions, material stiffness and shear;
- high-speed adaptive physical leaf trajectories;
- cold/warm parity, adverse dense-contact and topology changes;
- A/A timing controls and independent hardware/solver replication;
- actual CCD event completeness rather than broadphase-only counts.

A performance benefit that vanishes against fair cached IPC
is an **engineering baseline result, not an algorithmic
breakthrough**. Preserve that negative outcome without changing
the measured question or pruning inconvenient scenes.
