# CBP Phase 3 — developmental evidence and limitations (17 Nov 2026)

**Status: prototype, not SIGGRAPH/TOG novelty or end-to-end physics evidence.**

## Mac Phase-2 three-way crossover

| 256-object case | Exhaustive ms | Shared BVH ms | Certified cache ms |
|---|---:|---:|---:|
| Spaced | 0.047823 | 0.010755 | **0.008239** |
| Clustered | 0.060216 | 0.040233 | **0.030154** |
| Dense | **1.375221** | 1.592096 | 1.409058 |

These measurements are from the user's deterministic synthetic replay, not solver workloads.

## Phase-3 causal manager

Source: include/ccp/cbp_cost_manager.hpp. A deterministic 16-object spatial sample, sampled motion, prior paid graph-maintenance counters, and observed whole-query costs inform the choice of exactly one cross-region backend. Hysteresis and cooldown guard switching. On a switch the manager resets dormant caches, so reactivation/migration is charged in the public query time. No material labels, unseen future cost, or concurrent inactive-method probes are used.

Critical limitation: the current controller chooses a mode for the **whole cross-region query**, not independently for each topology-local patch. This is NOT the ultimate local-maintenance research hypothesis.

### Inspected synthetic CI transition data

Source: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44136667337 . 256 primitives, five 24-frame regimes, eight initial warmup frames, rotating execution order:

| Fixed exhaustive | Fixed shared BVH | Fixed certified cache | Causal manager |
|---:|---:|---:|---:|
| 0.456722 ms | **0.455899 ms** | 0.567405 ms | **0.431533 ms** |

Manager / best fixed speedup: **1.056x**, but this is one hand-designed synthetic, non-blinded test and not a confidence interval. There were three real switches. Critically, the rapid-deformation stage was a regression: manager **0.099697 ms** versus direct BVH **0.059393 ms**. A retrospectively added dirty-graph emergency-escape heuristic was tried; a different CI run gave only **1.006x** overall against the best fixed mode. Those runs cannot isolate code effects from runner noise. The heuristic was reverted. Do not hide this negative result or use the inspected sequence for final confirmation.

## Immutable material-replay bridge

Source: apps/cbp_kmcb_replay.cpp. Reads exporter-produced KYOTOM01 float32 current AABB snapshots already used by prior V4/V5 material replay. Six separate stateful policies: fresh SAP oracle, frozen V4 Guard/SAP bounded trial, fixed exhaustive regional, fixed shared BVH regional, fixed certified-cache regional, and causal CBP. The benchmark validates full current candidate-pair sets on every frame and emits summary plus optional frame trace CSV.

The KYOTOM01 format does **not** encode mesh connectivity or region labels. The initial replay partitions IDs into contiguous blocks only as a *surrogate*. This is not true topology-aware cloth patch assignment and is insufficient for a SIGGRAPH novelty claim. Input reading and CSV serialization are excluded from query timing; cold cache switches and selector time are included.

CI uses scripts/test_cbp_kmcb_smoke.py to verify parser, parity and output contracts on a generated fixture; this is NOT a material-scene performance evaluation.

## Next validation gate

First run 48-frame bounded pilots on actual immutable generator files on Apple Silicon, with A/A controls and matched inputs. Then compare on untouched real deformable mesh trajectories, topology-derived region ownership, strong published-family BVH baselines, and complete solver-step cost. Measure memory growth and quadratic dense-contact fallbacks explicitly. If routing cannot outperform V4 and the strongest fixed deployable alternatives after all switching costs, the hypothesis fails, regardless of appealing synthetic plots.
