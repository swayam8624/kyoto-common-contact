# RBC Stage-2 repeated ten-policy / A-A native development study

19 November 2026. **Exploratory development results, not independent test-set proof.**

Original executed source: GitHub Actions run
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44547606970
at commit 9ee5338344f18155cfb2e03a188f80a59aa9b986.

Original preserved archive:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44547606970/artifacts/14541936231

ZIP SHA-256:
568c5f9e2d2278b0476d5fa3d085f6020203e7b6f0241a8d0568c82cd100a841

## What this DOES and does not prove

The runner newly generated 12 KYOTOM01 material streams (six material
families x budgets 128 and 512) using explicitly different motion,
geometry and topology seeds from the old 54 material tests.
All original source SHA-256 receipts and native full-pair CSV SHA-256
receipts were independently checked against the ZIP: 12 of 12
original binary sources and 12 of 12 actual benchmark CSVs passed.

Each source has 60 original frames, warmup eight, then 52 timed
frames. On EACH scene 10 native full-pair algorithms were independently
instantiated and run for FOUR whole-trajectory repetitions, alternating
forward and reversed method execution orders. The source pair ID hash
was checked across all 10 methods on EVERY timed frame and repetition.
Total post-warmup native method/frame records = 12 x 4 x 10 x 52
= **24,960**. Original source binary snapshots were NOT altered.
No external hardware result or GPU proxy was inferred.

This controls for some order effects; it does NOT create 12 fully
independent physical solvers, remove CPU thermal/OS variance,
or provide ABBA independent session statistics. The 12 newly
generated streams have been inspected now and are DEVELOPMENT data.

## Mean Guard A/method native current-candidate query ratios

This is per-scene mean query time, averaged across four repetitions
per method, geometric mean across the 12 original source scenes.
Values above 1 favor method over Guard A.

| Actual executed method | Guard A/method geomean | Scenes faster than Guard A |
|---|---:|---:|
| Guard .045 A | 1.000000x | reference |
| Guard .045 B (identical A/A) | 1.000156x | 5/12 |
| Guard .100 | 0.999703x | 4/12 |
| Guard .200 | 0.811403x | 3/12 |
| Indexed RBC .045 | 0.693634x | 3/12 |
| Indexed RBC .100 | 0.692032x | 3/12 |
| Indexed RBC .200 | 0.620021x | 3/12 |
| Rebuilt SAP | 0.539450x | 2/12 |
| Persistent SAP | 0.570477x | 4/12 |
| Original frozen V4 | **1.049371x** | **10/12** |

**A/A control:** geometric Guard A/Guard B = 1.000156x,
while the largest absolute single-scene difference from 1
was about 2.5053%. Thus a 5% geometric ratio cannot be
promoted to significance without more independent replication.

These particular indexed RBC results are a SEPARATE engine and
should NOT be substituted for the flat batched-repair RBC measured
in the 54-scene six-policy study. The ten-policy pilot varies
skin .045/.100/.200 and includes A/A, whereas the six-policy
cohort includes node-indexed, flat RBC and real physics.

## Twelve actual source conditions: no suppressed failures

| Material | Actual N | Guard/RBC .045 | Guard/RBC .100 | Guard .100 ratio | Guard/V4 | Guard A/B |
|---|---:|---:|---:|---:|---:|---:|
| cloth | 128 | 0.678x | 0.688x | 1.018x | 0.987x | 0.986x |
| cloth | 512 | 0.897x | 0.919x | 0.997x | 1.153x | 0.994x |
| sponge | 216 | 0.414x | 0.398x | 0.865x | 1.054x | 1.025x |
| sponge | 512 | 0.523x | 0.476x | 0.849x | 1.022x | 1.001x |
| cushion | 128 | 0.422x | 0.402x | 0.858x | 0.970x | 1.004x |
| cushion | 512 | 0.265x | 0.246x | 1.114x | 1.079x | 1.009x |
| soft_ball | 256 | 0.835x | 0.880x | 1.222x | 1.023x | 0.998x |
| soft_ball | 1024 | 1.177x | 1.183x | 0.924x | 1.025x | 1.005x |
| spring | 126 | 1.419x | 1.497x | 0.921x | 1.000x | 0.993x |
| spring | 504 | 1.544x | 1.613x | 0.905x | 1.265x | 0.991x |
| mixed | 191 | 0.463x | 0.475x | 1.498x | 1.030x | 0.997x |
| mixed | 727 | 0.843x | 0.852x | 0.993x | 1.018x | 0.998x |

These results reinforce MATERIAL-SPECIFIC behavior:
an RBC indexed certificate can win convincingly on spring
and high-N soft-ball, yet lose massively on sponge,
cushion and small mixed scenes. Simply switching the
skin from .045 to .100 does NOT cure all the failures;
.200 worsens aggregate performance. There is no
credible universal skin parameter or static universally
winning RBC algorithm established.

## Research decision and next benchmark

- Do not modify fixed V4 or historical sources.
- Keep the stronger FLAT RBC candidate and the original node-indexed
  algorithm as an explicit negative-control comparison.
- The all-method empirical ranking is NOISY; do not train
  a gating model on these 12 development cases then call
  the same cases heldout.
- A future causal controller may choose flat RBC only after
  measuring REAL short-window costs with complete probe
  overhead charged; best-per-scene hindsight is not a baseline.
- First improve physical workloads: the existing 9 coupled
  scenes produced 81,390 sphere contact corrections, but
  zero naturally generated same-cloth self-contact. The
  new nonteleporting fold_over experiment is an independent
  direct attempt to challenge that gap.
- New external contact-rich IPC/CCD solver tests, balanced
  A/A sessions and full-step costs are required before
  discussing paper-level general-performance gains.

The full source-verified 54 native material/9 coupled physics
study is documented separately in
STAGE2_NATIVE_54_AND_9_VERIFIED_RESULTS.md.
