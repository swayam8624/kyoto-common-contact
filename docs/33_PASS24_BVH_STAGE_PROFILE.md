# Pass 24 — Source-backed BVH stage profiling on two operating systems

## Why profiling was necessary

The careful independent four-arm Pass 22B timing comparison established
that equally cached-input official IPC Toolkit LBVH remains much faster
than the current selective hierarchy. Pass 23 then tested skipping
incident-edge/face mark allocation at high motion fractions, with
only sub-1% and order-sensitive wins on Ubuntu/macOS. Another
optimization of the box bookkeeping without a profiler would be
guesswork.

The new, deliberately OPTIONAL `RefitHierarchy::QueryStages` receipt
partitions one real native C++ candidate query into five consecutive
intervals:

1. full source topology and collision filter signature computation;
2. safe source swept feature box construction/reuse;
3. cold BVH construction or warm node refitting;
4. full vertex–face plus edge–edge hierarchy traversal;
5. candidate sort, deduplication and output normalization.

The five exact integer nanosecond stage deltas must SUM to the
instrumented total native wall time on every C++ source row.
Each instrumented output must equal the independent exhaustive
VF/EE source oracle, and cold/warm epoch labeling is verified.

## Original physical source

The original neo-Hookean FEM finite rigid-triangle solver generates
three real different mesh/material/drift variants (12, 20, 24 major
segments, 360 accepted physical macro-steps per source). Thirty
macro-step intervals per scene are sampled.

The original 20 m/s fast impact contributes three independently
accepted adaptive physical leaves. It is not blended into routine
macro-step interpolation. Four independently progressing trials
measure both full and selective hierarchies.

This is `apps/pass24_stage_profile.cpp`; it does NOT fabricate a
motion trajectory or measure the whole physics solver.

## Actual native results

[Pass 24 stage-attribution CI](https://github.com/swayam8624/kyoto-common-contact/actions/runs/45405191891)

The values below are fractions of the native INSTRUMENTED query,
not percentages of complete solver time:

| Stage | Ubuntu full | Ubuntu selective | macOS full | macOS selective |
| --- | ---: | ---: | ---: | ---: |
| VF/EE traversal | 73.72% | 73.39% | 77.54% | 76.02% |
| Candidate normalization | 11.66% | 11.54% | 10.12% | 10.01% |
| Tree build/refit | 5.92% | 6.08% | 5.36% | 5.28% |
| Swept box handling | 5.76% | 6.08% | 4.59% | 6.25% |
| Topology signature | 2.93% | 2.90% | 2.39% | 2.43% |

Both CI operating systems passed native compilation, independent
exhaustive VF/EE parity, stage-sum verification, original FEM
trajectory generation, receipt-parser validation and artifact upload.

**The bottleneck is overwhelmingly hierarchy traversal.**

## Next engineering hypothesis

The current dual-tree VF traversal expands **four child-node pairs**
each time two nonleaf boxes overlap. A conservative but potentially
cheaper alternative is to split only one of those overlapping
internal regions, choosing the one with the largest current AABB
extent. This produces two child pairs, not four.

This is a standard spatial tree traversal heuristic, **not a novel
collision detection theorem**. The product of two node sets is
partitioned exactly when either the left or the right node is split,
so pair completeness is maintained as long as no newly overlapping
subregions are discarded and the primitive leaf filter remains
unchanged.

The edge/edge self-tree traversal has an analogous choice between
splitting the first or second off-diagonal internal node. The
diagonal LL/LR/RR path must remain exact and must never pair an edge
with itself.

A scientifically useful next A/B trial must retain the old
split-four strategy as a full-cost comparator, query both at every
physical FEM state once, counterbalance execution order, measure
identical-source candidates, and eventually repeat against pinned
IPC Toolkit v1.6.1 cached-input LBVH.

## Explicit limitations

- Source solver integration is EXCLUDED from candidate stage timings.
- Opt-in chrono reads and reporting have nonzero overhead. Do not
  compare these instrumented method totals directly to the
  uninstrumented official IPC ABBA wall times.
- The physical states are correlated and come from one solver family.
- A faster custom dual-tree walk is not automatically publishably
  novel. Prior-art and robust new contact scheduling remain open.
