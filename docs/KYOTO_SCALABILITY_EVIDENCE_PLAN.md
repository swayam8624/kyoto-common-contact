# Kyoto scalability research plan — evidence before claims

Status: **experimental protocol, not measured proof**. Created after the November 2 faculty feedback. Preserve all original evidence; do not rewrite old charts.

## What the existing evidence establishes

In the 2026-11-02 three-repeat coupled benchmark, `online_contact_work` beat `sap_rebuild` but lost to `global_guarded` for each of 1, 2 and 3 cloths. The current fixture family is too narrow to infer a scaling crossover. The isolated AABB benchmark and the physically coupled solver step benchmark are different metrics and must never be ranked together. Full-step multi-cloth times include periodic exhaustive audits.

## Three implementation obstacles to remove before making scale claims

1. `OnlinePartitioner::stableId`, `reset` and `regionForCell` assume **1–3 sheets**, **12×12 mesh cells**, and partition spans **12, 6, 3**. These fixed bounds make a higher-resolution or many-sheet scaling sweep invalid as presently implemented. Replace with configurable per-sheet dimensions, independent stable identifiers that cannot collide with primitive IDs, and a partition tree whose refinement levels follow the actual mesh topology.
2. `apps/multicloth_benchmark.cpp` hardcodes **1–3 cloths, two spheres, seven policies, 8 warm-up frames**, and 12–600 total frames. Expose explicit scene configurations, random seeds and repetitions. Record frame-level work and timing, not only per-policy averages.
3. The online controller's fixed work thresholds, cooldown windows and contact heuristics were tuned on small fixtures. Thresholds must be normalized or experimentally calibrated using a training set distinct from held-out validation scenes. Never tune to the final comparison set.

## Research hypotheses (falsifiable)

- H1: Under *localized, nonstationary contact* and high primitive counts, adaptive patch changes may reduce candidate processing and total CPU broad-phase time relative to persistent DBVT, incremental SAP and global guarded reuse.
- H2: Under *low contact density, mostly coherent motion*, global guarded reuse or incremental SAP may remain faster. This is a prediction, not a failure to hide.
- H3: The online controller's extra bookkeeping is only worthwhile when saved query cost exceeds control, rebuilding, invalidation and memory overhead; measure the cross-over in milliseconds, not just pair counts.
- H4: At extreme dense/all-overlapping scenes no broad phase can avoid the quadratic-sized true overlap output. Distinguish output-size limits from algorithmic overhead.

## Benchmark matrix

Vary one factor at a time, then use combined adversarial scenes:
- Scale: 1k, 4k, 16k, 64k and, if memory/time allow, 256k AABBs. Report total primitive counts exactly. Avoid claiming 256k *cloth vertices* unless a physically simulated cloth mesh actually has that resolution.
- Distribution: uniform sparse; dense cluster; many disjoint clusters; sliding contact front; concentrated cloth-fold hotspot; many moving small obstacles; all-overlap output-bound stress.
- Motion: static, coherent translation, local deformation, rapid phase change, teleport/reinitialization (explicit cache invalidation), mixed velocities.
- Temporal profile: 500+ sampled frames per scene when affordable, with separate warm-up. Include repeated changes rather than a single initial transient.
- Physics integration: first synthetic AABB query scaling, then real deformable cloth with progressively finer mesh and increased sheets/obstacles. Physics fidelity and stability must be verified separately.
- Independent random seeds, ≥5 repetitions, randomized algorithm order, and at least two hardware configurations where available.

## Required baselines

- Exhaustive AABB oracle (correctness; not a competitive performance baseline at large N).
- Rebuild SAP and **incremental SAP** (do not pretend rebuild SAP represents all sweep-and-prune algorithms).
- Actual Bullet **persistent** DBVT, plus fresh DBVT only as a construction-cost control.
- Existing global guarded reuse.
- Existing fixed/adaptive region variants and online approach.
- Spatial hashing / uniform grid only once implemented and verified with comparable input/output/collision masks.

Use identical immutable snapshot sequences for all isolated algorithms. Time **construction, updates, query and candidate materialization** consistently; also record them separately. Warm persistent structures using identical history. Keep candidate output semantics and collision filtering consistent.

## Correctness contract

For each frame generate oracle set `O` = every eligible intersecting padded AABB pair; candidate set `C` must satisfy `O ⊆ C`. Zero missed pairs is mandatory. Count `|C - O|`, duplicate pairs, invalid IDs, cache invalidations, and memory use. The oracle runs **outside timed query measurements**. For selected smaller cases also verify actual narrow-phase contacts, continuous crossings if CCD is introduced, and solver trajectories against a common reference. Never imply discrete AABB validation proves continuous collision detection.

At large N, the oracle can be expensive: conduct out-of-band oracle verification on sampled frames and independently generate known overlapping-pair witnesses, clearly labeling unsampled frames as not fully audited. AABB correctness is required for every method on every *audited* frame, and no algorithm can be called universally correct from spot checks alone.

## Timing/reporting

Measure separately:
- isolated broad phase: build/update/revalidation/controller/query/candidate materialization; include end-to-end measured CPU wall time.
- coupled solver: broad phase, narrow phase, constraints/physics, optional audit, end-to-end CPU step. Compare audits-on with audits-on and audits-off with audits-off.
- memory peak and temporary allocations; candidate count and precision; controllers' splits, merges, cache evictions and strategy switches.
Report medians, p95, per-run distribution, confidence intervals computed over independent runs, and actual hardware/compiler/optimization flags. Clearly state where method wins, ties, and loses. A crossover only counts if performance repeats on untouched held-out scenes.

## Implementation and verification sequence

1. Add characterization tests for currently hardcoded dimensions and observed output; record the baseline before refactor.
2. Generalize sheet/cell dimensions and patch-ID namespace; add exhaustive partition-coverage, uniqueness, split/merge, reset, cache-invalidation and overflow tests.
3. Build a reproducible *snapshot generator* with deterministic seeds and scene metadata; verify every algorithm consumes identical serialized snapshots.
4. Add missing strong baselines and independent oracle; record output coverage and policy fairness.
5. Add a frame-level CSV and runner with randomized order/repetition and hardware manifest.
6. Run small-scale screening, then large-scale sweeps. Use profiling to identify whether the online method's time is spent in partition maintenance, pair enumeration, certificates or memory traffic.
7. Improve the algorithm based on failure analysis **only on training scenes**. Run final unseen validation; publish negative as well as positive results.
8. Integrate selected validated scenarios in the native interactive demo; capture actual frames alongside paired CSVs. A screenshot alone is not proof.

## Presentation outline (build only after results are available)

1. Research question and why broad phase matters
2. What SAP, incremental SAP, DBVT and guarded reuse already solve
3. Exactly what the existing approach adds
4. Why we *suspect* adaptive partitioning may help in a restricted workload class
5. Instrumented pipeline and correctness guarantee
6. Dataset and scene construction, with visual previews
7. Strong baseline fairness and timing boundaries
8. Coverage/oracle tests and physical stability limitations
9. Small benchmark results, including our losses
10. Scaling curves with independently repeated measurements
11. Crossover or **no crossover**—whatever data show
12. Ablations: controller off, split only, merge only, traversal modes
13. Memory/performance trade-offs and failure cases
14. Research contribution, limitations, next experiment

Each slide: one testable headline, readable figure, plain-language explanation and an explicit conclusion. No invented timing graphs. Every claim must trace to a raw CSV, commit ID, configuration and command. Use actual screenshots of the simulator rather than decorative illustrations.

## Acceptance gate for faculty-facing proof

A claim of superiority requires: tested current build; independently verified zero missed required pairs on audited frames; strict same-input comparisons against persistent DBVT, incremental SAP, global guard and fixed partition; ≥5 independent repeats; held-out scene validation; reproducible raw results; error bars; and explanation of the actual workload domain where it wins. If this gate fails, present the finding and algorithmic diagnosis, not a victory claim.
