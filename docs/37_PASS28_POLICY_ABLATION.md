# Pass 28 — Fully paid hierarchical policy ablation

## Experimental distinction
A new independent union-only control stores 16 bytes per node; the
relational certificate stores 32 bytes per node. Both traverse the same
spatial hierarchy. Original default behavior is unchanged.

## Native source experiment (pending CI verification)
Three real physically integrated neo-Hookean toroidal FEM configurations,
20 endpoint states each. Five assigned policy regimes produce 300 unique
geometry/policy combinations and six counterbalanced repetitions, for
1,800 rows and 5,400 independently checked full-paid queries.
All 6 permutations of three methods occur for each frame across trials.
Every candidate vector must equal exhaustive VF/EE candidates. All methods
must rebuild exactly on scene change or changing high-bit policies.
The timing includes topology signature, feature bounds, build/refit,
policy summary build, traversal, counter updates and normalization.
Physics generation and exhaustive validation are excluded.

Assigned masks are controlled stress conditions, not policies inferred from
the underlying solver, which is not recoupled. Endpoint snapshots describe
macro-step endpoint-linear paths. The directed cycle is intentionally
adversarial and permits no reciprocal pair. It is not a representative
physical contact scene. All-allowed is the compulsory negative control.

CI artifacts report full-paid times, cold/warm results, per-scene and
per-trial aggregates, and peak incremental summary vector capacity.
No result against IPC or whole-solver performance is claimed.

## Separate locally executed structural probe
A standalone cross-BVH AABB experiment (not this production FEM solver)
compared baseline, union, and relation over 5 regimes, 2 sizes
(128/512 leaves per tree), 9 frames, and 6 rotations. All 1,620
queries matched an exhaustive cross-product oracle. Warm-frame node visits
for 512 leaves per side, summed over 48 method queries:

| Regime | Baseline | Union | Relation |
|---|---:|---:|---:|
| All compatible | 3448896 | 3448896 | 3448896 |
| Same group | 3448896 | 2086704 | 2086704 |
| Directed cycle | 3448896 | 1430112 | 48 |
| Separated geometry | 48 | 48 | 48 |
| Neighbor groups | 3448896 | 2740608 | 2740608 |

The directed-cycle root rejection is the exact structural consequence
of the theorem, not evidence of general speed. Probe timings are
instrumented and nonproduction. Additional rejection was not observed
in three nontrivial/all-compatible ordinary policy comparisons. The
original spatial baseline is preferable where there is no policy work
to eliminate unless paid timing demonstrates otherwise.

## Reproduction
```sh
cmake -S . -B build-pass28 -DCMAKE_BUILD_TYPE=Release
cmake --build build-pass28 --parallel 4 --target kyoto_pass28_policy_fem_ablation kyoto_maintained_broadphase_fuzz_tests
ctest --test-dir build-pass28 --output-on-failure -R independent_seeded_3D_feature_broadphase_fuzz
./build-pass28/kyoto_pass28_policy_fem_ablation results/pass28/native
python3 scripts/report_pass28_policy_fem.py results/pass28/native/pass28_fem_policy.csv results/pass28/science
```

Do not promote before native oracle validation and replicated full-paid
measurements. A scientifically useful next design is a safe scheduler
that chooses between ordinary, union and relational paths using
policy sparsity and observed structural work, with selection cost paid
and held-out mixed workloads. Prior art review and genuinely coupled
deformable/deformable simulation remain necessary.
