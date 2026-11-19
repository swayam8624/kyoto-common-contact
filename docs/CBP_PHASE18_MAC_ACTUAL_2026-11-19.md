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
  three seed values (3203, 3217, 3251) per material/regime/size condition. The material generator uses seed % 101 as a phase offset in its forcing; geometry and topology are otherwise identical for a given material/size. These are phase-varied replicates, NOT independent initial-condition samples.
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

## Completed original 11-algorithm ranking (new Mac SHA-verified scorer)

The author then ran `scripts/analyze_kyoto_decision_coverage.py` on the
**same 39 already-completed** Mac replay CSVs. Its stdout reported:
`PASS: 11 methods verified without rerunning the experiment`.
The original decision manifest SHA-256 stayed
`5cd6965a73e2fe3417df01ffa46a56fe641b6b33d4b2e5c937f584afe4e9da8e`.
The independent report text was supplied in conversation. These are
user-side Mac measurements, not GitHub-hosted timings.

Natural fixtures: 36 phase-varied trajectories, eight warmup frames
excluded. Higher Guard/method is better. The **first-place** V4 algorithm
is a *small* 1.0306x geometric-mean improvement; each other tested
method, including original Online, was slower than standalone Guard on
the geometric mean of the chosen trajectory cohort.

| Executed method | Guard/method geomean | Wins vs Guard | Sum-of-scene-means speedup | Median of scene p95 ms |
|---|---:|---:|---:|---:|
| Frozen V4 Guard/SAP bounded trial | 1.0306x | 21/36 | 1.0316x | 1.14752 |
| Global Guard | 1.0000x | reference | 1.0000x | 1.18700 |
| Selective Guard | 0.8450x | 10/36 | 0.9248x | 0.98077 |
| Original Online | 0.7493x | 2/36 | 0.8143x | 0.95015 |
| Regional fixed | 0.7145x | 3/36 | 0.7916x | 0.61935 |
| Regional adaptive | 0.7018x | 3/36 | 0.7823x | 0.66538 |
| Regional hierarchical | 0.6974x | 3/36 | 0.7694x | 0.61623 |
| Guard/SAP measured probe | 0.6874x | 6/36 | 0.5624x | 0.93750 |
| Persistent SAP | 0.4728x | 4/36 | 0.5132x | 0.60177 |
| Fresh SAP | 0.4513x | 2/36 | 0.4947x | 0.63467 |
| External Bullet persistent DBVT | 0.2474x | 0/36 | 0.2923x | 1.26675 |

**Do not conflate metrics:** the sum-of-scene-means ratio weights
high-cost scenes more heavily; it is NOT end-to-end wall-clock speedup.
The median of each scene's P95 is NOT a pooled P95 nor a paired tail
speedup. Results include every candidate pair materialized and exact
pair fingerprints checked, but no solver/contact/CCD time.

**Artificial rigid translation, separated from natural fixtures:**

| Executed method | Guard/method geomean | Wins vs Guard |
|---|---:|---:|
| Persistent SAP | 2.7190x | 3/3 |
| Frozen V4 | 2.6567x | 3/3 |
| Fresh SAP | 2.5306x | 3/3 |
| Measured Guard/SAP probe | 2.4676x | 3/3 |
| Regional hierarchical | 1.4332x | 3/3 |
| Regional fixed | 1.4209x | 3/3 |
| Regional adaptive | 1.3642x | 3/3 |
| Original Online | 1.2864x | 3/3 |
| External Bullet DBVT | 1.0410x | 3/3 |
| Global Guard | 1.0000x | reference |
| Selective Guard | 0.9759x | 0/3 |

The artificial case strongly favors SAP because repeatedly invalidated
Guard envelopes create avoidable rebuilds. This does **not** change the
natural ordering and should not be marketed as a physical solver result.

The observed difference between V4 and Guard (roughly 3% on the natural
cohort) is too small to defend without replicated same-snapshot
measurements, A/A controls and policy-order balancing. The **next
experimental priority** is to measure that noise floor rather than to
invent a new algorithm, change frozen thresholds or re-export geometry.

## What the report DOES NOT establish

* The complete 11-policy ranking is now available from the separately
  verified local Mac CSV reanalysis above; it remains a one-run-per-method
  descriptive ranking, not replicated evidence of general superiority.
* It does not demonstrate that the on-policy V4 switch consistently wins
  across sponge or mixed material regimes.
* It does not establish any trained ML policy behavior; weights absent.
* It does not measure CCD, narrow-phase contacts, mass-spring end-to-end
  solver time, cloth self-interaction correction, or improved folding.
* It does not contain repeated A/A trials on one machine and cannot
  support a population-level confidence interval.
* Three different seeds here change the forcing phase but NOT the generated initial geometry or topology. They are not independent physical scene samples.
* Earlier V5/V6 synthetic results and later Phase 16 million-AABB
  digest/count-only results are different cohorts, so they are
  neither pooled nor overwritten.

## Inspect and rank all original policies without rerunning anything

From the repository root on the same Mac, after pulling the latest
research/kyoto-decision-coverage branch:

    python3 scripts/analyze_kyoto_decision_coverage.py --results output/kyoto-decision-mac-first --output output/kyoto-decision-mac-first/verified-all-methods-v1
    open output/kyoto-decision-mac-first/verified-all-methods-v1/VERIFIED_ALL_METHODS.md

The analyzer intentionally refuses overwritten output or tampered source
SHA-256, validates framewise SAP pair counts and hashes (not saved raw pair-ID lists) and original execution order,
and treats natural dynamics and artificial translation as separate cohorts.

Research recommendation: do not retune frozen V4 from these scenes.
Use these results as evidence for an adaptive cost-aware strategy only in
specific activation regimes, and conduct controlled repeated-seed,
repeated-order and A/A comparisons before generalization. Subsequent
scientific work must integrate actual coupled narrow-phase/CCD/solver
evidence and held-out trained-model comparisons.
