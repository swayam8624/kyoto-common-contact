# Phase 18 Mac: 36 natural material trajectories + 3 independent diagnostics

Evidence received 19 November 2026 from the author's Mac, together with a
DECISION_COVERAGE.json manifest. The original uploaded manifest SHA-256 is
5cd6965a73e2fe3417df01ffa46a56fe641b6b33d4b2e5c937f584afe4e9da8e.

Important: the manifest contains full per-trajectory Guard/V4 warm timing
ratios and SHA-256 receipts for the original full-pair replay CSVs, but
NOT all 11 individual method timings. The CSV files remain on the author's
Mac and were not uploaded to this repository. This document is an analysis
of the user-supplied manifest, not a claim of independent access to the
unuploaded CSVs.

## Actual workload and correctness contract

* 36 naturally stepped material trajectories (60 frames each) with
  three seeds (3203, 3217, 3251) per material/regime/size condition.
* 3 deliberately artificial whole-scene rigid-translation diagnostics.
* Original 11 CPU AABB candidate algorithms executed including real Bullet
  DBVT. Trained ML was NOT executed, and cannot be ranked.
* The runner's SAP parity check passed for each completed trajectory.
* Frozen V4 switching thresholds were NOT retuned.
* Method order was seeded per trajectory, with recorded execution slots.
* Actual N values are 512, 727, 1058, 1331 and 1382. They differ from
  the requested generation budget for cloth, mixed and sponge; actual N is
  the only valid criterion for an N>=384 trial.

## Natural motion: the strongest honest conclusion

Across all 2,160 natural frames, V4 actually executed 2,059 Guard frames,
101 SAP frames, 40 SAP trial frames and 7 backend transitions.
It used SAP at least once in 5 of 36 natural trajectories.

Using warmup-excluded Guard/V4 ratios already recorded in the immutable
manifest, the trajectory-level geometric mean is 1.030587x, favoring
V4 by ~3.1% on this specific collection. V4 won 21/36 trajectories.
This is a small overall advantage and must not be overstated.

In 31 trajectories where V4 selected no SAP frames, the geomean ratio
is 1.018531x and V4 won 18/31. Therefore, it is wrong to attribute every
positive relative timing to successful algorithm switching. Fixed method
order was randomized, but one run per policy/trajectory can still retain
thermal and cache confounding at these microsecond-to-millisecond scales.

In the five trajectories that DID use SAP, the geomean Guard/V4 was
1.108587x with 3 wins; switch activation is not universally profitable.

### Which natural trajectories actually explored and retained SAP?

| Material | Regime | Actual AABBs | Seed | Guard frames | SAP frames | Trial frames | Transitions | Warm Guard/V4 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| cloth | rapid | 1058 | 3203 | 32 | 28 | 8 | 1 | 1.3275x |
| cloth | rapid | 1058 | 3217 | 32 | 28 | 8 | 1 | 1.2763x |
| cloth | rapid | 1058 | 3251 | 31 | 29 | 8 | 1 | 1.1414x |
| sponge | baseline | 1331 | 3203 | 52 | 8 | 8 | 2 | 0.8781x |
| sponge | rapid | 1331 | 3203 | 52 | 8 | 8 | 2 | 0.9860x |

The THREE rapid cloth trajectories all went beyond their eight trial
frames and continued using SAP. They support a narrow positive result:
on the tested rapid-cloth fixture family, original V4 can reach its
trial, elect SAP and improve total timed broad-phase query duration.

The two sponge runs had exactly eight trial frames and two transitions,
returned to Guard, and showed no positive Guard/V4 result. These are
important negative examples of a correctly measured but unhelpful trial.

The total switch count is not the number of successful conversions
to long-term SAP. The most informative split is three sustained SAP
adoptions and two rejected SAP trials.

### Stratified source-derived comparison

Three seed values underlie EACH cell. Do not treat 60 adjacent frames as
60 independent observations.

| Material | Regime | Actual N | Guard/V4 geometric mean | SAP frames | Transitions |
|---|---|---:|---:|---:|---:|
| cloth | baseline | 512 | 1.0194x | 0 | 0 |
| cloth | baseline | 1058 | 1.0291x | 0 | 0 |
| cloth | rapid | 512 | 1.0547x | 0 | 0 |
| cloth | rapid | 1058 | 1.2458x | 85 | 3 |
| mixed | baseline | 727 | 1.0264x | 0 | 0 |
| mixed | baseline | 1382 | 0.9936x | 0 | 0 |
| mixed | rapid | 727 | 1.0027x | 0 | 0 |
| mixed | rapid | 1382 | 1.0095x | 0 | 0 |
| sponge | baseline | 512 | 1.0070x | 0 | 0 |
| sponge | baseline | 1331 | 0.9654x | 8 | 2 |
| sponge | rapid | 512 | 1.0532x | 0 | 0 |
| sponge | rapid | 1331 | 0.9853x | 8 | 2 |

## Artificial translation diagnostics (separate, never pooled)

Three deliberately artificial cloth rapid N=1058 motion sequences:
9 total Guard frames, 171 SAP frames, 24 trial frames and 3 switches.
All three had 3 Guard, 57 SAP and 8 trial frames. Guard/V4 warm
ratios by seed: 2.6433x, 2.6554x, 2.6713x. Aggregate geometric mean
is 2.656665x. This tests the mechanism's cache-invalidation response
and is NOT physical cloth or general solver speedup.

## What the report DOES NOT establish

* It does not give reliable cross-method speedup rankings for all
  11 executed policies; the original manifest omitted their timing
  summaries. A new additive no-rerun all-policy scorer is provided
  in scripts/analyze_kyoto_decision_coverage.py and MUST read the original
  Mac replay CSVs and verify their SHA-256 receipts before ranking.
* It does not demonstrate that the on-policy V4 switch consistently wins
  across sponge or mixed material regimes.
* It does not establish any trained ML policy behavior; weights absent.
* It does not measure CCD, narrow-phase contacts, mass-spring end-to-end
  solver time, cloth self-interaction correction, or improved folding.
* It does not contain repeated A/A trials on one machine and cannot
  support a population-level confidence interval.
* Three different seeds with a fixed fixture generator are not three
  independently calibrated physical experiments.
* Earlier V5/V6 synthetic results and later Phase 16 million-AABB
  digest/count-only results are different cohorts, so they are
  neither pooled nor overwritten.

## Inspect and rank all original policies without rerunning anything

From the repository root on the same Mac, after pulling the latest
research/kyoto-decision-coverage branch:

    python3 scripts/analyze_kyoto_decision_coverage.py --results output/kyoto-decision-mac-first --output output/kyoto-decision-mac-first/verified-all-methods-v1
    open output/kyoto-decision-mac-first/verified-all-methods-v1/VERIFIED_ALL_METHODS.md

The analyzer intentionally refuses overwritten output or tampered source
SHA-256, validates framewise SAP pairs and original execution order,
and treats natural dynamics and artificial translation as separate cohorts.

Research recommendation: do not retune frozen V4 from these scenes.
Use these results as evidence for an adaptive cost-aware strategy only in
specific activation regimes, and conduct controlled repeated-seed,
repeated-order and A/A comparisons before generalization. Subsequent
scientific work must integrate actual coupled narrow-phase/CCD/solver
evidence and held-out trained-model comparisons.
