# Phase 17 Mac evidence review — 19 November 2026

Status: completed user-side Apple Silicon run, from the complete terminal
transcript supplied in the conversation. Raw CSV and binary outputs remain
on the user's Mac under output/kyoto-phase17-mac-01; they have NOT been
uploaded into this Git branch. The numbers below are derived from the
printed results, not from unseen CSV fields.

## Correctness outcome

AppleClang 21 successfully built the whole targeted source set including
Bullet DBVT. Eight Bullet C99-extension warnings and three existing
multicloth test initializer warnings were nonfatal.

All four native CTests passed. All 36 material trajectories across six
materials and six regimes passed exact SAP candidate count/hash parity
against **11 actually executed CPU methods**, including Bullet.

The learned ML controller was NOT benchmarked with trained weights.
The successful learned C++ regression uses a synthetic deterministic
test model for ABI/correctness, not an evaluated learned predictor.

All six synthetic pressure scenarios passed independent count/fingerprint
comparison, actual rebuilt-grid and persistent-grid pair file emission,
and complete sorted pair-set equality. The offline viewer was generated.

## Material performance — all 24 console-logged frames, seed 61749

These Guard-relative geometric means were calculated across 36 trajectory
full-frame means. They are deliberately NOT labeled as the HTML dashboard's
warmup-excluded results.

| Algorithm | Guard/method geomean | Trajectories faster than Guard |
|---|---:|---:|
| Global Guard | 1.0000x | reference |
| Selective Guard | 0.7074x | 10/36 |
| Original generalized Online | 0.5637x | 8/36 |
| Regional adaptive | 0.5017x | 8/36 |
| Region hierarchical | 0.4990x | 8/36 |
| V4 Guard-SAP bounded trial | 0.4917x | 1/36 |
| Guard-SAP measured probe | 0.4845x | 1/36 |
| Regional fixed | 0.4814x | 8/36 |
| Persistent SAP | 0.4172x | 4/36 |
| Rebuilt SAP | 0.3681x | 4/36 |
| Bullet DBVT persistent | 0.1505x | 1/36 |

Actual primitive counts: cloth 128, sponge 216, cushion 128,
soft_ball 256, spring 126, mixed 191. Every trajectory has N<384.
Frozen V4 only starts SAP trials at N>=384 AND after three expensive
Guard rebuilds, a measured guard EWMA threshold, and an expired cooldown.
Therefore **NONE of these 36 Mac trajectories could exercise V4's SAP
switching path**. This is a real small-size guarded-path weakness, not a
legitimate test of adaptive switching at the scale targeted in V5/V6.

The historical V5/V6 independent confirmations on different synthetic
distributions favored V4 with Guard/V4 geometric means about 1.091x
and 1.083x. Results from neither family should overwrite the other.

## Warm 8-frame synthetic CPU pressure times

The following are frames 2–8 mean COMPLETE CPU query times (ms), including
preflight, maintenance, decision and count+hash enumeration, but excluding
real pair-file output and downstream solver time.

| Scene | 1-thread ms | 8-requested ms | Speedup | Actual workers |
|---|---:|---:|---:|---|
| sparse | 0.06454 | 0.06435 | 1.003x | 1 |
| slabs | 0.10334 | 0.09710 | 1.064x | 1 |
| burst | 1.70883 | 0.59116 | 2.891x | 1 or 8 |
| bridge | 9.36033 | 2.49351 | 3.754x | 6–7 |
| teleport | 2.52299 | 1.13136 | 2.230x | 1 or 8 |
| all_overlap | 0.52233 | 0.52112 | 1.002x | 1, X-sweep |

The pressure run uses 4,096 objects per scene except 512 for all_overlap,
NOT a million-object workload. Requested thread count is NOT actual worker
count. Sparse and slabs stay on one worker due to workload thresholds.
All-overlap is routed to a serial exact X sweep.

## Actual pair outputs, persistent versus independent rebuilt grids

These are pair occurrences ACROSS eight frames, not unique temporal pairs.
Both independently emitted full lists were audited equal on every frame:

| Scene | Pair occurrences | Bytes |
|---|---:|---:|
| sparse | 120 | 960 |
| slabs | 0 | 0 |
| burst | 517,337 | 4,138,696 |
| bridge | 918,592 | 7,348,736 |
| teleport | 881,493 | 7,051,944 |
| all_overlap | 1,046,528 | 8,372,224 |

In all_overlap, 512 objects produce 130,816 pairs EACH frame,
then 1,046,528 across eight frames.

## Measurement boundaries and next step

These are one-seed, fixed-method-order CPU microbenchmarks, not a
randomized independent trial. Small differences may be affected by
cache, power, scheduling and execution order. A physically coupled
cloth solver, narrow phase, CCD, actual folding or end-to-end frame time
are NOT measured by these reports.

Preserve frozen V4 and all historical negative results. Audit the existing
36-trajectory CSV decision fields directly with the new
scripts/audit_kyoto_phase17_decisions.py, without rerunning old data.
For new experiments, deliberately create N>=384 varied-contact
multi-seed trajectories, log **actual** Guard versus SAP activation
and migration, and randomize comparison order. A size-eligible run alone
does not prove the SAP trial was taken. A proper trained-model comparison
requires independently available valid ML weights, not smoke weights.
