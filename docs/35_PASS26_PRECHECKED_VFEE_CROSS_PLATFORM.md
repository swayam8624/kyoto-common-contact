# Pass 26 — Early child-AABB rejection in VF/EE traversal

## Motivation

Verified Pass 24 native source stage profiling showed that
VF/EE traversal consumes 73–78% of paid maintained-BVH query time.
Pass 25's standard split-larger-node heuristic failed on both
operating systems: reducing the branching factor to two was slower
than traversing both tree levels and pruning child products.

Pass 26 tests an alternative that retains the ORIGINAL four-child
VF traversal and left-first EE self-tree traversal but moves
an AABB overlap test **ahead of each recursive call**.

## Implementation

`RefitHierarchy(..., prechecked_children=true)` activates
`cross_prechecked` and `self_prechecked`. The flag is OFF by
default. All source feature boxes, topology epoch signatures,
spatial tree construction, selective leaf refitting, contact
masks, incident topology filtering and candidate normalization
are otherwise unchanged.

For a parent node pair already proven to overlap, its Cartesian
child products partition the same candidate space as before.
Each child pair is tested for AABB overlap before recursion.
A separated child is rejected immediately; a surviving pair
enters a helper that does not repeat the same node-pair check.
VF primitive leaves still use the complete original
`f::valid_pair` semantics, including incident and mask policies.

EE self-tree diagonal nodes still partition as `LL + LR + RR`,
with `LR` skipped only on current proven box separation.
No edge can pair with itself. The separate branch is prohibited
for maintained temporal-frontier or span-descent combinations
until cross-mode semantics are independently tested.

This is an ordinary BVH traversal implementation optimization,
NOT a new contact-detection algorithm or robust continuous
collision theorem.

## Native correctness

`tests/maintained_broadphase_fuzz_tests.cpp` requires the same
candidates as an O(E²+VF) independent source oracle across 1,120
three-dimensional adversarial cubic path cases: complete overlap,
isolated geometry, high coordinate scales, masks, inclusive
touching, topology epoch changes and restored nonfinite inputs.

`apps/pass26_prechecked_abba.cpp` generates *actual* original
physically integrated compressible neo-Hookean FEM source states
for three mesh/material/velocity variants. Four independently
allocated maintained BVHs are advanced once per physical source
frame: A1/A2 are early child-overlap prechecks; B1/B2 are original
per-recursive-call overlap checks.

Eight repeated method-instance trials rotate all four ABBA
execution orders. All four candidate sets on every measured native
FEM frame must equal the independent exhaustive source oracle.

Paid wall time includes topology signature, conservative feature
generation, BVH updates, VF/EE traversal, and normalization.
Original FEM physics is outside the compared clocks.

## Source-backed results

[Pass 26 original physical FEM Ubuntu/macOS CI](https://github.com/swayam8624/kyoto-common-contact/actions/runs/45407273113)

Ratio = **incumbent paid query / early-child-prechecked query**.
Above 1 favors the new opt-in method.

| Original native measurement | Ubuntu | macOS |
| --- | ---: | ---: |
| All warm-source FEM frames | **1.07230×** | **1.00391×** |
| Scene 0, 12-major FEM | 1.08904× | 0.9833× |
| Scene 1, 20-major FEM | 1.06882× | 1.0066× |
| Scene 2, 24-major FEM | 1.06558× | 1.0153× |
| Optimized A1 / A2 (A/A) | 1.00122× | 1.01761× |
| Incumbent B1 / B2 (A/A) | 1.00093× | 1.01169× |
| ABBA order-ratio spread factor | 1.02233× | 1.05060× |

Ubuntu's opt-in method used about 6.7% less full-paid time
and all eight trials favored it. macOS observed only about
0.4% less paid time, below the A/A and method-order effects;
some source meshes and repetitions regressed.

Therefore the improvement is **workload/platform dependent and
NOT a defensible cross-platform speedup claim**. It may
reflect differences in generated machine code, allocator, branch
prediction, processor/cache architecture, or benchmark noise.
Which cause dominates has NOT been established.

## Decision

- Preserve the new traversal as an **opt-in, verified experimental
  branch**, but DO NOT make it the engineering default.
- The original four-child traversal remains the independently
  validated baseline.
- Do not infer a win against official IPC Toolkit LBVH from
  this same-algorithm A/B. The strong cached-input official
  IPC comparison remains the decisive external benchmark.
- The most useful next evidence is branch/check counts and
  compensated, same-run official IPC A/B under the exact
  source FEM regimes, particularly the negative multi-body and
  fast-impact cases.
- Any publishability claim needs a much larger algorithmic
  contribution: novel safe scheduling policy, a fully coupled
  contact solver integration, matched narrowphase CCD events,
  fair per-solver total timings and independent prior-art review.
