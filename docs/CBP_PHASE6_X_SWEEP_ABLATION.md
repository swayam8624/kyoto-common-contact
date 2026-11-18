# Kyoto CBP Phase 6 — exact X-swept region-pair pruning

Status: developmental structural optimization, **not a new collision algorithm novelty claim**. Date: 18 November 2026.

## Motivation from the user's Mac 48-frame coupled cloth topology experiment

Two cloth sheets and one sphere (577 primitives), all methods exact and all four patch spans share frame-by-frame geometry fingerprints, 42 measured queries per configuration:

| Span | Regions | Frozen V4 mean ms | Regional exhaustive mean ms | Portfolio mean ms |
|---|---:|---:|---:|---:|
| 1 | 289 | **0.295924** | 0.347974 | 0.316529 |
| 3 | 33 | 0.288716 | **0.250660** | 0.302217 |
| 6 | 9 | **0.285836** | 0.378647 | 0.312788 |
| 12 | 3 | **0.276319** | 0.352907 | 0.297074 |

At span 3, regional exhaustive achieved a 1.152x ratio relative to V4 in a **single inspected 48-frame run** (13.2% lower mean query latency); the portfolio rejected its four-frame trial and then did not retry within 48 frames because the cooldown was 48 frames. This does not establish a reliably faster deployable algorithm or whole-solver improvement.

### Structural issue

The Phase-0 exhaustive cross-region join scans all unordered region-envelope pairs before testing primitive pairs. For 289 region guards, this means 289*288/2 = 41,616 region-pair iterations each frame, even when the guards are widely separated. At span 3, there are 528; at span 6, 36; at span 12, 3. The loop also accumulates local candidate costs inside each region, explaining the potentially non-monotonic optimum.

## Experimental exact sweep

`RegionRouter::setRegionSweep(true)` enables a separate X-axis sweep for the existing exhaustive cross-region mode. The implementation sorts conservative current region-union AABBs by min X, then for each region scans only later envelopes with min X <= current max X. It checks current Y/Z overlap and primitive collision masks as before. Skipping a pair with strictly non-overlapping X intervals is mathematically safe: no current 3D AABB pair can overlap between them. Touching at equal X remains included, matching the established exact-pair policy.

The original exhaustive code path remains selected when `setRegionSweep(false)` (default). V4, the Phase-3 controller and the Phase-4 portfolio are unchanged. A new independent `topology_xsweep` replay policy is separately timed as the **eighth** method against the original seven; old seven-policy CSV remains readable.

Exactness: `tests/cbp_region_sweep_tests.cpp` compares the original router, the swept router and independently exhaustive primitive oracle on sparse, dense, moving, changing region ownership, metadata updates, touching boundary and invalid-input cases; CMake integration and same-geometry eight-method replay were checked in GitHub Actions.

## First bounded CI experiment — not Apple Silicon results

Ubuntu GitHub Actions run: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44223518856 . Two cloth sheets, one sphere, 24 frames, three warmup, 21 timed; all four mesh patch spans share exact frame geometry.

| Patch span | Frozen V4 ms | Original exhaustive ms | X-swept exhaustive ms | Speedup old/sweep |
|---|---:|---:|---:|---:|
| 1 | 0.211677 | 0.539733 | 0.288412 | 1.871x |
| 3 | 0.214641 | 0.269975 | 0.269493 | 1.002x |
| 6 | 0.211005 | 0.424895 | 0.429206 | 0.990x |
| 12 | 0.213457 | 0.385754 | 0.376528 | 1.025x |

These are single developer CI timings on a shared runner, not a controlled statistical replication. Reduced region-pair enumeration clearly benefits span 1, but the new method **does not beat frozen V4 overall** on the 24-frame CI cohort. Its improvement is a cleanup/foundation, not a SIGGRAPH novelty claim.

## Temporal diagnostic — separate retrospective analysis

`scripts/analyze_cbp_topology.py` now reports a per-frame relative-timing chart and four fixed chronological windows. At 24 frames and span 3, the final window (frames 22–24) had old exhaustive/V4 ratio about 1.402x and swept/V4 about 1.440x, while the full 21 timed frames were negative. This justifies studying workload-dependent crossovers but is **hindsight descriptive** and cannot be fed into the on-policy controller or declared an online achieved speedup.

## Next research gates

1. Run the new eight-policy ablation on the already characterized Mac with separate output files (never overwrite previous seven-policy evidence) and compare 48-frame spans 1/3/6/12 on identical geometry. Detect any early/late crossover with fixed chronology, not retrospectively selected windows.
2. Establish repeated matched A/A controls, random invocation order and multiple independent scenes/seeds. Current coupled fixture is deterministic for a given configuration; a new dataset source is needed to create independent stress tests.
3. Profile measured `regionPairTests`, local SAP/exhaustive tests, cross-pair candidates, output sorting, allocations and refits. `regionPairTests` under sweep counts visited X-overlapping candidate region pairs, not the disjoint pairs skipped without work.
4. Compare published-quality broad phases and peak memory. Only then propose a true local contact-work-aware maintenance policy whose regions independently choose a kernel and coordinate cross-boundary queries exactly.
5. Integrate the strongest candidate into the real five-iteration solver and measure full frame step, contact stability, inter-sheet/self-collision, penetration and geometry divergence. The replay is post-step broad-phase only; it does not yet establish better physics.
