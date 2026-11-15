# Kyoto CPU scaling — exploratory reproducible measurement memo

**Status:** Observed raw CI measurements; **not publication-grade evidence** and **not proof that our regional online algorithm outperforms SAP**.

## Provenance

- Workflow: [Kyoto scaling references, run 43740562391](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43740562391)
- Git revision: `36823a795a3cd1d8195d7effa7e7311a0105d8ed`
- CPU crossover artifact: `kyoto-linux-cpu-crossover`, ID `14171608555`
- Prior 512-object regional artifact: run `43740128064`, ID `14171478524`
- Host: GitHub-hosted Linux runner (hardware details in raw `manifest.json`, not independently calibrated).
- Every reported number below is **the median of three run-level frame-time medians**. Each crossover repetition measured 6 frames; source has 3 repetitions; per-backend source CSVs retained in artifact. Caution: per-frame timings are not independent repetitions.

## Cross-over: full candidate materialization (ms, lower is better)

| Scenario | N | SAP serial | Fixed 4-thread SAP | Work-aware 4-thread SAP | Auto policy |
| --- | ---: | ---: | ---: | ---: | ---: |
| Cluster | 512 | 0.02337 | 0.11456 | 0.12129 | 0.02205 |
| Cluster | 4096 | 2.28061 | 0.98924 | 1.04566 | 1.04393 |
| Cluster | 16384 | 26.20702 | 11.52800 | 11.21751 | 11.25913 |
| Hotspot | 512 | 0.01314 | 0.10706 | 0.11003 | 0.01040 |
| Hotspot | 4096 | 1.26289 | 0.77592 | 0.76493 | 0.74645 |
| Hotspot | 16384 | 13.78265 | 7.61304 | 6.79336 | 6.77886 |

At 16,384 objects the auto strategy is ~2.33× faster than serial SAP on cluster and ~2.03× on hotspot. These are CPU multithreading speedups against the *same SAP algorithm family*, not speedups of the adaptive spatial region controller.

The 4,096-object fixed multithreaded SAP slightly beats auto and work-aware on the cluster. That loss must be reported too. At 512 objects the extra thread creation/merge dominates. The threshold in `cpu_sap_auto` is a preliminary engineering heuristic, not an independently fitted optimal policy.

## Regional findings at 512 objects (separate earlier exploratory run)

| Scenario | SAP serial | Hierarchical regional | Fixed flat regional | Adaptive flat regional |
| --- | ---: | ---: | ---: | ---: |
| Cluster | 0.01444 | 0.10900 | 0.71843 | 0.74817 |
| Hotspot | 0.00849 | 0.12939 | 3.67054 | 3.69468 |

The two tables are **different run cohorts** and should not be pooled into a single inferential analysis. Flat regional work is particularly costly; hierarchical pruning is necessary but not sufficient to beat SAP.

## What is scientifically missing

1. CPU and GPU **same-output, same-snapshot, same-precision** measurements with GPU actual runtime (the Metal compiler test passes, but no physical GPU result is collected).
2. Real incremental SAP, persistent Bullet DBVT, and broad-phase total cost with unchanged pair semantics on a held-out scale suite.
3. Online region partitioner with arbitrary resolution and verified parallel region cache updates; current coupled cloth simulation still has fixed 12×12 cells.
4. CPU method comparisons at a common benchmark host with hardware and thermal characterization, randomized algorithm order across runs, and sufficient independent trials for uncertainty bounds.
5. Proof of contact-pair correctness on all audited frames; larger samples currently use equality of serial/parallel SAP, not exhaustive oracle.

## Interpretation for presentation

State only that parallel SAP shows a **measured crossover** and that the *present* regional method remains behind. This guides the next optimization and avoids claiming that Kyoto's original proposed online partitioner already wins.

Produced from raw archived CSV, not a simulated estimate or fabricated table.

## Stronger temporal baseline — incremental SAP (later run cohort)

Workflow: [Kyoto scaling references, run 43741390835](https://github.com/swayam8624/kyoto-common-contact/actions/runs/43741390835), `kyoto-linux-cpu-crossover` artifact ID `14171634711`. The new `cpu_sap_incremental` keeps an X-sorted order and repairs adjacent inversions across frames. The implementation compares the **complete candidate pair vector** against rebuilt serial SAP every frame and against the exhaustive reference on selected smaller frames.

| Scenario | N | Serial rebuilt SAP | Incremental SAP | Four-thread fixed SAP | Work-aware SAP | Auto |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Cluster | 512 | 0.03410 | 0.04712 | 0.12898 | 0.12962 | 0.03087 |
| Cluster | 4096 | 2.20391 | 1.98513 | 0.97910 | 1.06134 | 1.05059 |
| Cluster | 16384 | 24.81983 | 24.13938 | 11.03380 | 11.25040 | 11.25665 |
| Hotspot | 512 | 0.01621 | 0.02010 | 0.11441 | 0.11451 | 0.01273 |
| Hotspot | 4096 | 1.27378 | 0.96550 | 0.77411 | 0.77106 | 0.76654 |
| Hotspot | 16384 | 13.60797 | 12.28578 | 7.55936 | 6.81086 | 6.80570 |

Times are again median of three run-level frame medians, in milliseconds. The new incremental baseline is stronger than full sorting on the 4,096/16,384-object scenes, but still slower than four-thread methods there; it can also be slower at 512 objects because the insertion-maintenance overhead is not free. It uses a simple adjacent inversion repair and is **not** a best-available published incremental SAP implementation. These are a *different CI run* from the table above: do not compare individual numbers across runs as a synchronized paired trial. GPU timings, the original adaptive-region controller's large-scale performance and independent cross-host replicates remain unmeasured.
