# Pass 20 — Selective physically moving 3D feature updates

## Experimental objective

A fully refitted bounding-volume hierarchy and official IPC Toolkit LBVH
are already measured as independent candidate-generation references. The
previous bounded Spacetime Contact Atlas performed better with a smaller
1× pair-frontier budget in some real-FEM runs but did not consistently beat
**official IPC with equally cached connectivity**. Those experiments
cannot establish general superiority or research novelty.

This pass attacks a different, sharply stated bottleneck: **static
or locally moving geometry should not have to regenerate every swept
feature box and every internal BVH bound on every physical step**.

The new implementation is additive, and the old full-refit hierarchy
remains an untouched experimental control. Both produce the same original
VF/EE candidate contract, not a physics response or CCD solution.

## Actual C++ source

- `include/kyoto/selective_feature_boxes.hpp`: new cold-validated
  vertex/edge/face dependency cache, with complete motion-change checks
  each frame and selective recomputation of conservative cubic-path AABBs.
- `include/kyoto/refit_feature_hierarchy.hpp`: optional selective
  dirty-leaf/dirty-ancestor BVH update; ordinary full refit stays default.
- `tests/selective_refit_tests.cpp`: independent exhaustive oracle
  comparisons for 40 triangles with only one moving face, 30 identical
  frames, 90 deforming cubic paths, all-motion fallback, collision skin,
  same-epoch masks, connectivity invalidation, nonfinite-input rejection
  and recovery.
- `apps/pass15_official_ipc_fem_benchmark.cpp`: independently measures
  the complete selective-query duration and checks every returned
  candidate against the original exhaustive reference on identical
  actual solver-generated FEM source states, alongside the previous
  algorithms and actual pinned IPC Toolkit v1.6.1.
- `scripts/report_pass15_official_ipc.py`: requires valid paid
  selective timing, exact candidate counts, conservative source
  statistics and per-scene measurements; never invents data.

## Precise safety argument

Let `P_v(t)` denote the four cubic Bezier controls for vertex `v`,
and `B_v(t, h)` the validated, outward-expanded control-hull AABB
with collision skin `h`. In exact arithmetic a Bezier path lies
within the convex hull of its controls. The existing `vertex_box`
performs finite-number validation and `nextafter` outward expansion.

At every query:

1. The full topology signature is regenerated. It covers source
   vertex identity and order, body/object labels, edge and face
   connectivity, filtering categories, collision masks, and the
   explicit topology epoch. Any difference forces a fresh original
   `features::build`, cold dependency rebuild and cold hierarchy.
2. On a topology-preserving query, every source vertex's four
   3D controls and the global skin are compared with the last
   successfully accepted input. **Only exactly equal paths with the
   same skin** reuse a previously validated swept box.
3. Every other vertex calls the original conservative `vertex_box`.
   All new boxes are validated before changing any accepted cache,
   so a nonfinite later vertex cannot leave a partly updated tree.
4. Every edge/face incident to at least one dirty vertex is
   reconstructed by conservative AABB union. All other features
   retain exactly the prior set of contributing unchanged source
   vertex boxes, and consequently the same bounds.
5. A hierarchy leaf points to one stable source feature record.
   The changed leaves are updated. Every ancestor of a changed leaf
   is refreshed bottom-up, with siblings' latest bounds. The nodes
   are stored in depth-first preorder; descending node indices
   therefore update a descendant before its ancestor. Unaffected
   ancestors remain correct.
6. When more than about a quarter of a tree's records are dirty, the
   implementation deliberately falls back to a complete refit.
   On the very common all-moving deformable mesh case this avoids
   expensive repeated ancestor-list construction.
7. The existing VF dual-tree and EE self-tree traversal then
   computes and normalizes the full source-compatible candidate set.
   No temporal heuristic, learned policy or selective motion flag
   is allowed to omit a candidate.

This is a **candidate-box completeness invariant** under the stated
topology, numeric and motion-control contract. It is **not** a
general double-precision continuous contact solver proof.

## Work that is still fully paid

Selective refitting does not make the entire pipeline sublinear:
the conservative current topology signature is still fully checked;
all vertex paths are compared; candidate traversal is performed on
the latest hierarchy; output filtering and normalization still run.
Only actual swept-box generation and affected bounds refitting can
shrink when the moving source is spatially localized.

When most physical nodes move, it may be **slower** than plain refit
because the query also pays motion comparisons and dependency tracking.
That outcome must be retained in the negative benchmarks, not hidden.

This method may be particularly suitable for solver workloads with a
large static rigid environment and relatively few actively moving
deformables. The currently validated physical torus plus rigid patch
has both moving and stationary vertices; no selective runtime advantage
is claimed until a green original-source CI receipt demonstrates one.

## Compile, test and reproduce

```bash
cmake -S . -B build-selective -DCMAKE_BUILD_TYPE=Release
cmake --build build-selective --parallel 4 \
  --target kyoto_selective_refit_tests \
           kyoto_refit_feature_hierarchy_tests
ctest --test-dir build-selective --output-on-failure \
  -R 'selective_3D_feature_refit_exhaustive_oracle_and_rollback|maintained_refit_hierarchy_VF_EE_oracle_and_invalidation'
```

For the actual official upstream source comparison, configure the
existing `KYOTO_WITH_OFFICIAL_IPC=ON` build and run
`kyoto_pass15_official_ipc_fem_benchmark`; the existing workflow
runs its original 30-snapshot/12-repeat, full 360-step/4-repeat and
heterogeneous real-material/topology cohorts. The paired CSV adds
`selective_refit_paid_ns`, `selective_refit_pairs`,
`selective_changed_vertices`, `selective_partial_refits`,
`selective_unchanged_refits`, and `selective_first`.
Results are scientifically interpretable only once native C++ and
source-receipt validation pass.

## Remaining acceptance gates

- A/A timing controls and method order must show any small
  observed difference cannot be explained solely by runtime noise.
- Independent physical solvers and *held-out* geometric regimes
  must reproduce a meaningful improvement, including static-rich
  scenes and the documented negative case of nearly every vertex
  moving. Do not tune exclusively against the current single FEM
  source.
- Full narrowphase/CCD event and entire **solver-step** costs must
  be measured separately; reduced AABB candidate time is not full
  contact detection speed.
- Published incremental BVHs, front tracking and selective update
  algorithms must be compared before asserting a novel contribution.

A passing implementation test establishes only its tested semantics;
a strong algorithmic or paper novelty claim still remains open.
