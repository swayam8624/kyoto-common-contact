# First real material-generator pilot — negative transfer result

**17 November 2026 — Apple Silicon macOS, from user-provided benchmark stdout.**

## Immutable input and exactness boundary

- Command: `ccp_material_export output/cbp-phase3-local/cloth-compression-n256-s7201.kmcb cloth 256 48 7201 compression`.
- Actual exporter result: **288 cloth triangle AABBs**, 48 consecutive snapshots, seed 7201, compression deformation regime; the requested budget of 256 rounds up to a 12 x 12 cell cloth producing 288 triangles.
- Test replay: `ccp_cbp_kmcb_replay DATA.kmcb 32 48 TRACE.csv`.
- 42 measured frames after 6 warmup; every compared method returned the same complete sorted current AABB candidate set as fresh rebuilt SAP. Last output reported 1,805 exact candidate pairs.
- Region ownership is **contiguous index blocks of 32**, not validated mesh-connected spatial partitioning. Material generator is an uncalibrated discrete mass-spring fixture, not a physically validated cloth solver. No continuous collision test or complete solver-step speedup is implied.

## Frozen V4 versus Phase-3 CBP

| Method | Mean ms | P95 ms | V4/method |
|---|---:|---:|---:|
| V4 bounded trial | **0.081062** | 0.201625 | 1.000000 |
| Regional exhaustive | 0.083804 | 0.112417 | 0.967291 |
| Causal manager | 0.102176 | 0.145708 | 0.793357 |
| Regional shared BVH | 0.104131 | 0.149333 | 0.778465 |
| Regional certified graph | 0.191091 | 0.296375 | 0.424208 |
| Fresh SAP | 0.126887 | 0.177458 | 0.638855 |

CBP manager selected direct shared BVH in **all 42 measured frames** and switched **zero** times. There is no adaptive benefit in this pilot. V4 took about 20.7% less time than the manager (equivalently, CBP latency is about 26.0% higher).

### Critical interpretation: this small V4 run is a Guard-only control

The frozen `GuardSAPBoundedTrial` is permitted to enter SAP trials only when `boxes.size() >= 384`, AND after a streak of expensive rebuilds. Here N=288, so V4 necessarily remained on its Global Guard backend. Its reported 0.081062ms is not a demonstration of active Guard/SAP switching.

## Scientific consequence

This is a **negative transfer** from hand-coded spatial fixtures to the existing deformable material generator. In a contact-rich cloth snapshot, regional exhaustive nearly matches Global Guard, while the shared BVH and potential-edge cache introduce more overhead than they save. The synthetic 4-6x wins must NOT be carried over as real-material speedup claims.

Absolute times and P95 are from one 48-frame local execution. No error bars, A/A runs, random order-reversed repetitions or independent real-cloth datasets were measured.

## Required next actions (pre-registered, not yet claims)

1. Instrument the existing replay without modifying any backend: emit actual V4 actions/trial status, manager sampled density/motion, actual regional local/cross tests, graph size and invalidation count. Keep query timing boundaries and existing data untouched.
2. Run a new trace on the **same** immutable pilot file (a diagnostic, not held-out validation). Do not overwrite the prior trace.
3. Then run a bounded cloth **512 target budget** (512 actual triangles) so V4's 384-primitive eligibility is satisfied, and confirm whether V4 actually enters SAP using the recorded actions. Eligibility alone is not evidence it switched.
4. Evaluate a larger 2048-target compression scene and unrelated motion regime before deciding if topology-local structure buys anything. Use matched baselines and new untouched seeds for a true confirmation gate.
5. Until region membership comes from topology and geometry, no claim of efficient *local* maintenance is justified. If the measured cross-over is absent, prefer V4 and reconsider the architecture rather than tuning heuristics on known scenes.
6. Critical architecture gap: the current causal controller cannot select the frozen V4/Global Guard policy at all. Its mode set contains only Regional Exhaustive, SharedBVH, and CertifiedCache. The final deployable orchestrator must incorporate a separately accounted exact Guard/V4 fallback instead of making its default compete only with weaker regional alternatives. Do not alter the frozen V4 source or invent a free hot-cache switch.
