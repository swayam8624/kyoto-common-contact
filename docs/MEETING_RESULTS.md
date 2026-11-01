# Kyoto meeting: post-fix benchmark results

**Generated from real CSV:** `output/native/live_benchmark-postfix.csv`

The independent all-triangle correctness oracle ran in a separate, untimed
preflight. The timed simulations use **candidate-driven narrow phase**, with
the exhaustive per-iteration oracle disabled. The final all-triangle scan for
the live visualization/minimum gap remains enabled, measured separately.

| Measurement | Rebuild every iteration | Guarded reuse |
|---|---:|---:|
| Measured steps | 1350 | 1350 |
| Mean step (ms) | 2.2087 | 0.9334 |
| Median step (ms) | 2.2446 | 0.8699 |
| p95 step (ms) | 2.5379 | 1.1285 |
| Max step (ms) | 4.2304 | 2.3691 |
| Mean broad (ms) | 1.5864 | 0.3172 |
| Mean precise path (ms) | 0.1070 | 0.1027 |
| Mean display diagnostics (ms) | 0.0195 | 0.0173 |
| Mean precise tests per step | 5358.1778 | 5857.9185 |

Observed mean step-time ratio (rebuild / guarded): **2.366×**.

**Interpretation:** limited to this fixed scene, collision type, timestep,
query margin, CPU and implementation. This is not IPC/OGC/CCFR speedup evidence.
The diagnostic scan must be removed or separately modeled before calling these
numbers a fully optimized production pipeline. A disabled oracle does not mean
zero unchecked misses; the dedicated oracle preflight and automated tests
provide the tested fixture's correctness evidence.

Generated figure: `output/native/benchmark-postfix.svg`
