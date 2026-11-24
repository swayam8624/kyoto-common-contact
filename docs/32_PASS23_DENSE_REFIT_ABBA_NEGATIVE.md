# Pass 23 — Dense selective-envelope regeneration: controlled negative result

## Why this experiment was necessary

Pass 22B's isolated A/B/B/A on real, solver-integrated FEM established
that the pinned **cached-input official IPC Toolkit LBVH** was about
22% faster than the maintained selective BVH in both full-trajectory
and multi-scene experiments. Small apparent advantages in earlier
many-method comparisons vanished after method-order correction.

The obvious local optimization was to skip repeated edge/face adjacency
marking in the selective feature cache when most vertices move.
This experiment tested that idea, instead of assuming it was helpful.

## Implemented source and independent controls

`include/kyoto/selective_feature_boxes.hpp` adds an **opt-in**
`dense_motion_fast_path` switch, off by default. When at least
half of the source vertex cubic trajectories change, it reconstructs
every current edge/face AABB from the latest validated swept vertex
boxes in a linear scan rather than allocating and populating the
incident-edge/incident-face marking sets.

The opt-in algorithm intentionally over-updates some unchanged features.
A conservative updated envelope cannot miss an admissible VF/EE
candidate. All other geometry, incident filtering, masks, topology
epoch invalidation, hierarchy traversal and output normalization are
identical to the existing production-style selective algorithm.

`apps/pass23_dense_refit_abba.cpp` uses original accepted physical
neo-Hookean FEM states from **three different native mesh resolutions
and material/drift combinations**, not scripted mesh animations.

A1/A2 are two independent optimized hierarchies; B1/B2 are two
independent incumbent selective hierarchies. Every instance receives
each physical source state exactly once. Orders rotate through all
four ABBA permutations, and every resulting candidate set is compared
to the independent exhaustive VF/EE reference.

`tests/selective_refit_tests.cpp` validates both algorithms on
stationary paths, one moving triangle, whole-mesh motion, masks,
contact skin, topology changes and invalid-input recovery. Strict
Python source-report tests reject missing candidates or nonpositive
paid clocks.

## Original cross-platform experimental results

Native CI: [Pass 23 cross-platform 4-arm ABBA run](https://github.com/swayam8624/kyoto-common-contact/actions/runs/45404046755)

The source consists of 3 mesh/material variants, 30 sampled
physically accepted FEM intervals per variant, and 8 four-arm repeated
measurements: 720 total paired rows (696 warm, 24 cold).

**Comparison metric: incumbent full-paid query / opt-in dense fast-path
full-paid query. Values greater than 1 favor the opt-in algorithm.**

| Measured scope | Ubuntu | macOS |
|---|---:|---:|
| All warm original FEM states | 1.00772× | 1.00151× |
| Scene 0, 12-major real FEM | 1.00588× | 0.98584× |
| Scene 1, 20-major real FEM | 1.00778× | 1.00974× |
| Scene 2, 24-major real FEM | 1.00875× | 1.00387× |
| Optimized A1 / optimized A2 timing | 1.00105× | 0.97767× |
| Incumbent B1 / incumbent B2 timing | 1.00544× | 1.00959× |
| ABBA order-ratio spread factor | 1.0902× | 1.0293× |

All four method instances match the exhaustive candidate sets on
every measured original physical frame. The dense fast path did
execute; this is **not** a failed or absent optimization.

## Interpretation — not a win

The measured ~0.8% Ubuntu and ~0.2% macOS gains are smaller than
same-algorithm and method-order effects. Only Ubuntu happened to
favor the new implementation in all 8 repeated trials; macOS did not.
Both machines show substantial order sensitivity.

**Do not promote this fast path as a demonstrated optimization.
Keep it opt-in, and keep the incumbent unmodified as the reference.**

It is also not novel: bulk-refreshing all feature bounds is a
standard engineering trade-off. The experiment's contribution is a
negative control showing that removing incident-mask bookkeeping is
not the dominant paid cost of this physical FEM broadphase.

These numbers must never be compared across machines as if they were
paired observations. Both platform results come from their own
measured native receipt artifacts.

## Next research gate

Measure paid time **within** the maintained hierarchy, under a
separate instrumented build:

1. complete topology signature comparison,
2. swept feature box generation,
3. changed-leaf or full internal BVH refitting,
4. VF and EE product-tree traversal and incident filtering,
5. pair sorting and normalization.

The stage measurements must sum to the externally paid total query
duration, and the instrumented candidate output must remain exactly
equal to the independent exhaustive oracle. Track the same stage
fractions on small, medium, large, contact-rich and fast-impact FEM
regimes before deciding on another algorithmic change.

Do not claim a full solver speedup: original physical FEM integration
remains outside these candidate-generation timers.
