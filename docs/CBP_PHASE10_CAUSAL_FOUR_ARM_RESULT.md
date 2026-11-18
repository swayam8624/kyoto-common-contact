# Phase 10 — Causal four-arm portfolio and route-visible exact real-mesh replay

**Status:** Experimental. All Phase 9 frozen reference algorithms remain unchanged. This is NOT an established graphics research novelty or a coupled deformable physics result.

## Motivation — concrete Phase 9 limitation

The existing replay configured `CbpPortfolio::setTrialKernel(CrossMode::Exhaustive)`; the original benchmark's online portfolio could NEVER choose SharedBVH, even when SharedBVH was fastest. The macOS 25-model/120-frame glTF experiment measured SharedBVH 6.59082 ms, old portfolio 19.23742 ms and V4 30.65575 ms (separately measured full CPU candidate query times). On a synthetic 1,000,000-AABB, 120-frame separated-island Mac test the mean exact local query was 6.6933 ms; independent global brute oracle was not run at 1M and the scenario is NOT 1M deformable objects.

## New independent C++23 controller

`include/ccp/cbp_portfolio_v2.hpp` exposes `CbpPortfolioV2` with four physically executed choices: FrozenV4, SharedBVH, RegionSweep and RegionExhaustive. Every frame executes exactly ONE choice; backend reset and migration are charged in the query. Sequential trials last six frames after an eight-frame baseline warmup. Promotions require the full charged trial average to be at most 94% of a previously observed champion EWMA. Rejected candidates return to the champion. Trials recur with bounded cooldown. Every region ownership or object-count change invalidates learned statistics. The manager cannot use the reference CSV, scene material names, future positions or the offline best method.

This is a conservative causal scene-level router. It is NOT per-patch mixed execution, a stable contextual bandit or an adversarially robust optimizer. Cross-scene/regime drift can bias the adjacent, historical timing comparison, and the ongoing sequential probes cost real frames. A more rigorous controller must explicitly address those limitations rather than hiding them.

## CI results from first 120-frame held-out real-geometry run

Actual successful CI: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44282416171

| CPU query policy | Mean ms (Ubuntu CI) | V4 / method |
|---|---:|---:|
| Fixed SharedBVH control | **8.18094** | **5.019x** |
| New causal four-arm portfolio | **15.85440** | **2.590x** |
| Old two-choice V4/regional portfolio | 25.91612 | 1.584x |
| Frozen V4 | 41.05732 | 1.000x |

All candidate sets were exact on each of the 120 frames. The new on-policy controller actually ran SharedBVH for 50 frames, RegionSweep 45, RegionalExhaustive 12 and V4 13. It performed seven trials, promoted three, rejected four and switched 11 times. Its mean was approximately **1.938x the cost of the best fixed SharedBVH** on this case. A separate small 371-AABB, 12-world frame sequence favored FrozenV4 (0.27599 ms) over the new portfolio (0.37904 ms). Thus the new policy is NOT uniformly better than the fixed baselines.

These are comparisons among separate process runs on the same exact geometry, not a paired randomized clock-controlled statistical study. Hardware/OS/caches/order and frame-to-frame candidate growth can all affect timing.

## Authentic 3D scene and decision visualization

The Phase 9 solid-mesh viewer now accepts an optional `portfolio-v2.csv`. It validates every frame's stable-ID pair hash, candidate counts, inter-model count and mode against the exact frozen SAP result before embedding a true four-arm timeline. Each frame reveals which algorithm ACTUALLY executed, whether it was a trial, return or promotion, and its measured on-policy wall cost. Colored bars are NOT an offline per-frame oracle. The viewer still displays imported source GLB triangles at higher visual detail independently of the lower-resolution collision triangle sampling.

## Reuse the completed macOS real-GLB recordings — NO physics/Metal reruns

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/common-broadphase-orchestrator
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu \
  --target ccp_cbp_portfolio_v2_tests ccp_cbp_portfolio_v2_replay --parallel 4
ctest --test-dir build/kyoto-final-cpu \
  -R '^cbp_four_arm_portfolio_oracle$' --output-on-failure

CASE="output/cbp-real-wave-mac-120"
if [[ ! -f "$CASE/snapshots.kmcb" || ! -f "$CASE/cpu-core.csv" ]]; then
  echo "Missing completed immutable real-glTF source or CPU reference: $CASE"
else
  if [[ ! -e "$CASE/portfolio-v2.csv" ]]; then
    ./build/kyoto-final-cpu/ccp_cbp_portfolio_v2_replay \
      "$CASE/snapshots.kmcb" "$CASE/instances.csv" \
      "$CASE/portfolio-v2.csv"
  fi
  if [[ ! -e "$CASE/FOUR-ARM-MAC-RESULTS.md" ]]; then
    python3 scripts/analyze_cbp_portfolio_v2.py \
      "$CASE/portfolio-v2.csv" "$CASE/cpu-core.csv" \
      --report "$CASE/FOUR-ARM-MAC-RESULTS.md"
  fi
  if [[ ! -e "$CASE/FOUR-ARM-REAL-3D-OBSERVATORY.html" ]]; then
    python3 scripts/render_cbp_gltf_scenes.py "$CASE" \
      --html "$CASE/FOUR-ARM-REAL-3D-OBSERVATORY.html"
  fi
  open "$CASE/FOUR-ARM-MAC-RESULTS.md"
  open "$CASE/FOUR-ARM-REAL-3D-OBSERVATORY.html"
fi
```

This tests the new controller on the **already recorded, verified 15,774 triangle-AABB/120-frame real Khronos GLB scene on the user's Apple Silicon CPU**. It preserves the previous independent CPU tournament and all three actual Metal benchmark files. Since this is a new temporal controller, its CPU query needs an actual fresh execution, but the expensive GLB download, geometry replay and GPU measurements need not be repeated.

## Next falsifiable research gates

1. Stop excessive stale-metric exploration by measuring regret and migration cost under independent multi-seed scenes, plus explicit cheap scene-statistic regime-change signals from the selected backend only.
2. Test a single shared exact spatial index with local per-region algorithm choice; do NOT rely on keeping several global indices hot for free.
3. Compare to real external optimized libraries and couple candidate queries to deformable solver outputs, self-collision/CCD and stability diagnostics.
4. Run repeated randomized benchmark ordering, report P50/P95/P99/memory and confidence intervals, quantify cold starts, candidate emission and pair sorting as separate charged costs.
