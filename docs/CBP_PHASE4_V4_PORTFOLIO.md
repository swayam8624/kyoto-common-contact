# CBP Phase 4: frozen V4 safety baseline and on-policy regional trials

Development branch: `research/common-broadphase-orchestrator`. Status: **experimental**, not a novelty or performance claim.

## Motivation: two actual Apple Silicon cloth-compression pilots

Both datasets were produced by the same lightweight discrete mass-spring material generator. They contain **AABBs only**, not mesh connectivity. Region labels are a 32-consecutive-ID grouping, NOT an independently validated cloth patch decomposition.

| Primitive count | Frozen V4 | Region exhaustive | Region shared BVH | Region certified graph | Existing CBP manager |
|---|---:|---:|---:|---:|---:|
| 288 | **0.078530 ms** | 0.080551 ms | 0.103835 ms | 0.186461 ms | 0.099275 ms |
| 512 | 0.222311 ms | **0.142434 ms** | 0.160647 ms | 0.577003 ms | 0.162908 ms |

These are single short observed development runs, **42 timed frames each**, with cold-prefix warmup excluded and exact sorted candidate-pair parity checked on every frame. Regional exhaustive was **1.561x** as fast as V4 at 512, but **0.975x** at 288. There are no confidence intervals or solver-level speed claims.

Crucially, V4's `GuardSAPBoundedTrial` permits its trial only at **N >= 384**, with additional 3 consecutive expensive Guard rebuilds and a cost threshold. In BOTH provided pilots its actual `action` log was **global_guarded_exact on all 42 measured frames**; neither pilot exercised a V4 SAP trial. No one should present the 512 comparison as defeating V4's active selector under switching pressure.

The previous CBP manager independently chose the shared BVH on every measured frame, for both datasets; no adaptivity was demonstrated. At 512 the direct BVH pruned ~27,404 potential cross-region primitive tests down to ~2,477 but still ran slower than exhaustive, indicating traversal overhead can dominate pair-comparison reduction. The certified cache had median ~5,052 potential pairs and ~95.5 dirty objects/frame; rebuilding graph neighborhoods was not amortized.

## Architecture

`include/ccp/cbp_portfolio.hpp` adds `CbpPortfolio`, a **separate seven-policy replay arm**. Frozen V4 and the Phase 3 controller source are UNMODIFIED. The portfolio executes exactly ONE backend per frame:

- It starts on the frozen V4 Guard/SAP implementation and observes real paid public-call time for 12 frames.
- It then executes a **four-frame regional trial** using exactly one developer-selected regional backend. This currently defaults to `RegionalExhaustive`, chosen as a DEVELOPMENT design response to the inspected 512-cloth result. No claim of unbiased algorithm discovery is permissible.
- It accepts the trial only if all four trial frames, including cold query initialization, average at most 85% of the previous V4 EWMA. If rejected, it returns to V4 next frame and pays the cold reactivation cost.
- After a successful regional trial, a scheduled later trial of V4 checks whether recent regional performance still justifies keeping regional routing. There is no simultaneous per-frame oracle or hidden evaluation of dormant methods.
- The controller enforces a 48-frame cooldown between trials, validates all input geometry before changing its internal state, and rebuilds upon primitive-count changes.
- Regional shared BVH and certified-cache methods remain selectable through `setTrialKernel`, but are NOT silently trialed by default. A fully automatic multi-region, multi-kernel orchestrator remains **unimplemented**.

Because the two development pilots influenced the initial regional candidate selection, these datasets MUST be treated as **training/development**, never as clean validation. A new uninspected seed cohort, different material classes, topology-derived patch labels and full solver runs are mandatory before any causal improvement claim.

## Verification and research boundaries

- `tests/cbp_portfolio_tests.cpp` exercises all three trial kernel variants, tests independently exhaustive exact pair parity across 120 varying frames, label changes, metadata changes, deformed geometry, invalid-input recovery, cold fallback and primitive-count reset.
- `apps/cbp_kmcb_replay.cpp` now compares seven methods on immutable input, including frozen V4, the old CBP manager, and new V4+regional portfolio. Every method is independently timed with rotating order; the portfolio does NOT receive the reference runtimes.
- Trace records `portfolio_mode`, `portfolio_trial`, `portfolio_accepted`, `portfolio_rejected`, `portfolio_v4_action` and charged `portfolio_switch_ms` separately from all old columns. `scripts/analyze_cbp_pilot.py` retains support for old six-method immutable traces.
- `scripts/test_cbp_kmcb_smoke.py` validates seven-policy CSV shape and exact parity on a generated KYOTOM01 fixture, including overwrite refusal and corrupt data. CI tests run optimized builds and sanitizers.

### What this is not

This is not per-region independent routing or a novel spatial maintenance theorem. The four-frame trial compares **adjacent but different frames**, so geometry nonstationarity and thermal/OS scheduling can confound its decision. The promotion threshold and initial candidate kernel are DEVELOPMENT heuristics inspected after previous pilot results. Reporting an observed win on these same inputs would be **in-sample**, not independent proof.

### Next scientific gates

1. Preserve the existing 288 and 512 original trace files. Rerun the same fixed datasets only to diagnose the new portfolio decisions; do not claim confirmation.
2. Pre-register untouched seeds and independent material deformation classes, then repeat with randomized invocation order, A/A controls, fixed warmup semantics and bootstrapped paired scene-level uncertainty.
3. Replace contiguous-ID groupings with actual mesh connectivity plus geometric locality. Evaluate contacts between patches and across separate meshes using conservative cross-region joins.
4. Add published-quality BVH (e.g. IPC-toolkit or Bullet implementation), exact candidate parity and peak memory comparisons; score all candidate filter/migration costs.
5. Integrate with solver step, compare real self-collision, deformation validity, total frame time, and separate narrow phase/CCD correctness. If no consistent gain, retain frozen V4 rather than promoting a complex manager.
