# CBP Phase 1–2 controlled diagnostic — 17 Nov 2026

**Exploratory engineering evidence; NOT independent research validation.**

Source: [GitHub Actions CBP exact spatial index regressions](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44132673592), workflow commit d3d7bce87b8ef3d29549aa23e73bd04bc8d1ee05. Ubuntu CI optimized C++23, NOT the user's Apple Silicon machine.

All three independently stateful routers processed the SAME 64 frames, rotating method call order. First 8 frames were excluded; remaining 56 frames were timed with whole RegionRouter.query() calls, including all local and cross-region work, output normalization, index refits and cache maintenance. The scenarios are deterministic synthetic geometry, NOT calibrated physics.

## Three-way timings, 256 objects

| Scenario | Exhaustive ms | Shared BVH ms | Certified cache ms | BVH speedup | Cache speedup |
|---|---:|---:|---:|---:|---:|
| Spaced, interleaved region IDs | 0.049093 | 0.013288 | **0.010335** | 3.695x | **4.750x** |
| Clustered, moderate contact | 0.081149 | 0.054934 | **0.044023** | 1.477x | **1.843x** |
| Dense, heavy contact | **1.961516** | 2.092256 | 2.682688 | 0.938x | **0.731x** |

## Instrumented mechanism

- Spaced case: exhaustive checked 1,605,632 cross-region primitive pairs; shared BVH checked 0. There were zero exact cross pairs.
- Clustered case: exhaustive checked 1,605,632 pairs; direct BVH checked 32,716; cached-potential filter checked 41,938.
- Dense case: all three checked 1,605,632 cross-region primitive pairs, so BVH traversal and graph maintenance were wasted overhead.
- The direct BVH refitted 11,480 changed leaves / 14,280 ancestors over measured frames; the certified graph's envelope index refitted 410 changed leaves / 510 ancestors.

The separate certified graph exact oracle passed 160 dynamically changing snapshots including quiet reuse, new pairs after envelope escapes, changing masks, stale-edge deletion, degenerate/touching boxes, ownership remapping and independent exhaustive parity. ASan and UBSan passed in GitHub Actions.

## What is established, and what is not

**Established in these limited tests:** exactly matching current-AABB candidate sets on the tested fixtures, dramatically less cross-region primitive checking when spatially separated, envelope-repair reuse on stable geometric bounds, and substantial losses for the cached graph when all objects overlap.

**Not established:** general performance on unseen geometry, statistically significant speedup on real simulation, Apple Silicon comparative performance, novel prior-art differentiation, memory scaling, continuous collision safety, faster whole cloth timesteps or improved physical fidelity.

This is a real crossover signal motivating cost-sensitive routing: the same algorithm is not best for all spatial interaction densities. A manager cannot label material types or look up the future fastest method; it must use causal current/previous cost signals and include migration, cold-start, switch, pair-filter and graph-memory costs. The graph can grow quadratically in dense cases.

## Next falsifiable experiment

Use independent real deformable trajectories and N=256/512/2048 cohorts, multiple order-reversed repetitions and an A/A control on Apple Silicon. Generate motion transitions INSIDE each scene and compare a charged-cost, cooldown-constrained manager against each fixed backend, original V4 and strong V5 published-family reference baselines. Record per-frame total solver time, candidate exactness, graph size, node traversal, maintenance and switch costs, plus appropriate CCD and self-intersection validation for any physical-quality claims. Retain all negative results.
