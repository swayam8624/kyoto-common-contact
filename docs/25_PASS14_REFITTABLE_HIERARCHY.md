# Pass 14 — Refittable dual-tree 3D candidate hierarchy (first implementation)

## What has actually changed

The preceding experiment had only exhaustive candidate enumeration, rebuilt
sweep-and-prune and a *persistent sorting permutation*. A sorted list is not
a spatial tree. Pass 14 adds a genuine maintained binary AABB hierarchy for
the existing typed **vertex/face/edge** contract, in
`include/kyoto/refit_feature_hierarchy.hpp`.

There are three trees, over vertices, faces and edges. Their leaves map
to stable feature-index records; interior nodes bound all descendants.
The initial topology constructs a deterministic spatial median hierarchy.
Subsequent source frames reconstruct conservative swept **feature boxes**
from original cubic control points, then bottom-up refit the stored nodes,
without rebuilding the tree topology.

Vertex–face queries perform a genuine **two-tree traversal**:
pairs of non-overlapping tree nodes are rejected as a whole; overlapping
interior nodes subdivide; at the leaf, the exact established category,
mask, incident-vertex and inclusive 3D AABB candidate contract is checked.

Edge–edge queries use a single-tree unordered self-join. For an interior
node it visits left–left, left–right and right–right; different subtree
pairs subdivide and ultimately visit each unordered leaf pair exactly once.
Self-edge pairs and incident edges are not valid candidates.

Every returned candidate is explicitly normalized and sorted. Broadphase
coverage is checked against the existing independent exhaustive oracle,
not against the sweep implementation.

## Cache invalidation and paid costs

The preserved spatial hierarchy is only valid while the feature signature
is unchanged. A signature includes the mesh topology epoch, vertex object
labels, each feature's ID, connectivity, category and collision mask.
Changing any value **rebuilds** the hierarchy. Changing only motion control
points refits it. The control tests include an unchanged-epoch mask
mutation and explicit epoch and feature deletion, all compared with the
independent candidate oracle.

Each `query()` must pay native mesh validation, swept-box extraction,
signature calculation and comparison, full hierarchy rebuild **or**
whole-node refit, traversal, pair filtering and normalization.
The approach therefore does **not** secretly reuse old candidate sets
or wave away cache-maintenance overhead. It does not yet implement
subtree-level/local refits, selective frontier persistence, or topology
epoch contracts against an actual production contact solver.

## Proving feature candidate safety

The standalone `kyoto_refit_feature_hierarchy_tests` currently specifies
180 reproducibly randomized 3D cubic-path warm frames plus cold,
collision-mask, topology-epoch and deletion cases; every one is checked
against `features::exhaustive`. The native physically integrated 3D
FEM benchmark independently checks hierarchy parity at every sampled
accepted solver state. Contact-bearing states are included.

A passing CTest is a regression and limited evidence for **candidate-set
parity** on these fixtures. It is not formal binary64 geometric
certification, a continuous collision detection proof, or contact physics.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kyoto_refit_feature_hierarchy_tests \
  kyoto_pass13_reference_fem_benchmark --parallel 4

ctest --test-dir build \
  -R maintained_refit_hierarchy_VF_EE_oracle_and_invalidation \
  --output-on-failure

./build/kyoto_pass13_reference_fem_benchmark results/pass14/native
python3 scripts/report_pass13_fem_baselines.py \
  --input results/pass14/native/native_fem_reference_broadphase.csv \
  --output results/pass14/science
```

The experiment CSV records `hierarchy_total_ns`,
`hierarchy_rebuilds`, and `hierarchy_refits` alongside the independently
measured exhaustive, rebuilt SAP and persistent-order SAP costs.
The reporting script plots all four measured pipelines. The Ubuntu/macOS
workflow adds the hierarchy correctness CTest before making plots.

**No unexecuted timing figure is interpreted as a performance result.**

## Required next gates

1. Integrate source solver **accepted physical leaves** rather than
   just once-per-macro-step sampled endpoints. Each source leaf must
   preserve its exact linearized motion and actual elapsed time.
2. Feed the **identical** accepted source motion/feature topology to
   official pinned IPC Toolkit LBVH and this maintained hierarchy;
   measure all conversion, build/refit, candidate extraction, and
   downstream narrowphase effort.
3. Validate full candidate/CCD event coverage against independent
   exact-feature enumeration. If official IPC returns extra conservative
   pairs, report them as such; never silently impose false equality.
4. Develop a truly selective maintained frontier and
   local-refit/subtree-rebuild strategy. Compare its worst-case
   invalidation and dense-cluster behavior against this full-refit
   hierarchy, persistent SAP and rebuilt SAP.
5. Counterbalance per-method ordering, add A/A trials, cold/warm runs,
   held-out workloads, independent replication and uncertainty intervals.

The scientific claim is deliberately constrained: **a real refittable
dual-tree candidate hierarchy exists in source code; correctness and
performance remain subject to successful independent execution.**
