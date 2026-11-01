# Kyoto meeting: post-fix benchmark results

**Generated from real CSV:** `output/review-2026-11-02/after/single.csv`

The independent all-triangle correctness oracle ran in a separate, untimed
preflight. The timed simulations use **candidate-driven narrow phase**, with
the exhaustive per-iteration oracle disabled. The final all-triangle scan for
the live visualization/minimum gap remains enabled, measured separately.

| Measurement | Rebuild every iteration | Guarded reuse |
|---|---:|---:|
| Measured steps | 1350 | 1350 |
| Mean step (ms) | 2.1946 | 0.9256 |
| Median step (ms) | 2.2168 | 0.8745 |
| p95 step (ms) | 2.5232 | 1.1256 |
| Max step (ms) | 2.6998 | 1.5626 |
| Mean broad (ms) | 1.5695 | 0.3142 |
| Mean precise path (ms) | 0.1093 | 0.1032 |
| Mean display diagnostics (ms) | 0.0196 | 0.0175 |
| Mean precise tests per step | 5358.1778 | 5857.9185 |

Observed mean step-time ratio (rebuild / guarded): **2.371×**.

**Interpretation:** limited to this fixed scene, collision type, timestep,
query margin, CPU and implementation. This is not IPC/OGC/CCFR speedup evidence.
The diagnostic scan must be removed or separately modeled before calling these
numbers a fully optimized production pipeline. A disabled oracle does not mean
zero unchecked misses; the dedicated oracle preflight and automated tests
provide the tested fixture's correctness evidence.

Generated figure: `output/review-2026-11-02/after/single.svg`
