# Kyoto Phase 18 Mac — post-hoc A/A source-verified robustness

Date of source report: 19 November 2026.

Evidence boundary: the author supplied the output of the existing
scripts/analyze_kyoto_phase18_aa_robustness.py on the Mac. The script
rechecked original native replay CSV SHA-256 receipts and strict framewise
SAP candidate count/hash parity. No native C++ timings or original material
simulations were rerun. This page preserves reported Mac results, not a
claim that the GitHub runner reproduced the Mac performance numbers.

## Overall decision from 144 repeated replay timings

36 original material trajectories times 4 randomized whole-trajectory
orders each = 144 timed runs, with two independent Global Guard and
two independent original frozen V4 policy instances per replay.

Natural Guard/V4 geometric mean across both copies: 1.0188x.
Conditional timing-repeat resampling envelope: 1.0117x to 1.0257x.
This envelope is conditional on the fixed Mac material geometries,
forcing-phase seeds and previously observed order repetitions; it is
NOT a confidence interval for new materials or physical workloads.

A/A controls: Guard median abs(log A/B) 0.02152, 95th percentile
0.16280; V4 median abs(log A/B) 0.02374, 95th percentile 0.11051.
These indicate measurable time variation even between identical code.

The direct recorded execution-order audit proves the old A/A label
confound: Guard A earlier / B earlier 144/0, V4 A earlier / B earlier
144/0. Original A/B identities were assigned AFTER shuffling, so every
A/B contrast confounds copy label with early/late sequence position.
The earlier independent-cache and correctness evidence survives; the
A/A distribution CANNOT be described as balanced early/late scheduling.

## Concentration and leave-one-condition-out sensitivity

With 12 equal-weighted condition cells, each with three forcing-phase
values and four timing repeats, the main benefit is nearly entirely
localized to the previously observed rapid cloth N=1058 fixture class.

Full natural fixed suite: Guard/V4 = 1.0188x.
Excluding cloth/rapid/N=1058: Guard/V4 = 1.0027x.
Conditional fixed-suite timing-repeat resampling envelope without that
condition: 0.9954x to 1.0106x. This interval includes the tie value 1.
Exclusion is a POST-HOC sensitivity calculation, not a valid prospective
method for discarding unfavorable/positive cases to manufacture a win.

The original rapid-cloth N=1058 fixture condition alone:
Guard/V4 = 1.2139x, conditional timing-repeat envelope
1.2042x to 1.2255x, zero V4 A/B decision disagreements. This
represents approximately 17.6% lower measured broad-phase query time
on these fixed snapshots; it is NOT yet evidence that the same benefit
occurs on independent initial geometries, topologies or coupled solvers.

## All natural conditions — reported per-cell repeat envelopes

| Material | Regime | Actual N | Guard/V4 | Conditional envelope | A/B action-difference frames |
|---|---|---:|---:|---|---:|
| cloth | baseline | 512 | 1.0224x | 0.9988x .. 1.0523x | 0 |
| cloth | baseline | 1058 | 0.9919x | 0.9446x .. 1.0206x | 0 |
| cloth | rapid | 512 | 1.0020x | 0.9737x .. 1.0279x | 0 |
| cloth | rapid | 1058 | 1.2139x | 1.2042x .. 1.2255x | 0 |
| mixed | baseline | 727 | 1.0092x | 0.9792x .. 1.0411x | 0 |
| mixed | baseline | 1382 | 1.0111x | 0.9759x .. 1.0431x | 0 |
| mixed | rapid | 727 | 0.9982x | 0.9926x .. 1.0047x | 0 |
| mixed | rapid | 1382 | 1.0004x | 0.9908x .. 1.0088x | 0 |
| sponge | baseline | 512 | 1.0099x | 0.9920x .. 1.0304x | 0 |
| sponge | baseline | 1331 | 0.9729x | 0.9603x .. 0.9845x | 7 |
| sponge | rapid | 512 | 1.0105x | 0.9964x .. 1.0259x | 0 |
| sponge | rapid | 1331 | 1.0025x | 0.9755x .. 1.0377x | 0 |

## Seven exact A/B decisions that diverged

All seven differences came from one existing Mac timed replay:
sponge, baseline, actual N=1331, forcing-phase seed 3203, repeat 2.
Every differing frame is AFTER the eight-frame timing warmup.
V4 copy A chose SAP, V4 copy B chose Guard. Across this replay,
copy A selected SAP in 15 frames, copy B in 8 frames.

| Frame (0-based) | A action | A measured ms | B action | B measured ms |
|---:|---|---:|---|---:|
| 11 | SAP | 1.755458 | Guard | 2.711500 |
| 12 | SAP | 1.800084 | Guard | 0.145291 |
| 13 | SAP | 1.744167 | Guard | 0.134958 |
| 14 | SAP | 1.772917 | Guard | 2.908125 |
| 15 | SAP | 1.850583 | Guard | 0.135250 |
| 16 | SAP | 1.946417 | Guard | 0.112292 |
| 17 | SAP | 1.877167 | Guard | 0.105208 |

Calculated directly from the reported 7 frame times:
A SAP total 12.746793 ms, average 1.820970 ms,
median 1.800084 ms.
B Guard total 6.252624 ms, average 0.893232 ms,
median 0.135250 ms. Guard B is extremely cheap on five of seven
frames and expensive on two (frames 11 and 14).

These observations are consistent with a Guard path that alternates
between cheap cache reuse and costly refresh, but the provided per-frame
record does NOT expose internal V4 metrics.builds, guardEWMA,
guardRebuildStreak, cooldown, or alternative counterfactual cache costs.
It is therefore NOT justified to say exactly which frames rebuilt
the envelope or which controller action was globally optimal.
Candidate pair fingerprints remain correct in both outputs.
V4's correctness is not synonymous with choosing the cheapest method
on every frame.

Frozen V4 code in include/ccp/final_broadphase.hpp enters SAP
trial only after at least three expensive Guard rebuilds
(elapsed >=0.25 ms each), Guard EWMA >=0.35 ms, and N >=384;
it can return to Guard after a losing trial or after further
SAP/motion state tests. Controller decisions use online elapsed
measurements and independent state, not hindsight oracle comparisons.

## Source and next scientific decision

Frozen original V4 and all old Mac outputs stay unchanged. Later code
corrected optional A/B identity scheduling by assigning identities
BEFORE shuffling, and the new runner advertises
aa_labels_shuffled_independently. This is a correction for FUTURE
measurements, not a retroactive repair of this 144-run Mac result.

No new repeated run is required to conclude that general natural
broad-phase superiority is unproven: outside the one promising
cloth/rapid/N=1058 fixture the selected natural suite is essentially
tied at the noise resolution.

To advance the Kyoto study, prioritize genuinely new scene geometry
and topology, broader deformation and collision scenarios, fair
unmodified external baselines, and eventually coupled solver,
continuous detection and mesh-penetration/visual correctness.
Do not tune V4 on this repeatedly inspected fixed fixture.
