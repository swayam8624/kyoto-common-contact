# Kyoto 54-scene held-out rapid benchmark — source-verified native result

Date: 19 November 2026. Branch: research/kyoto-decision-coverage.

## Ground truth and provenance

[Completed CI](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44468375567), artifact name kyoto-heldout-geometry-contact-evidence, ZIP SHA-256 4224c8c13123e45a3bccb9a75cd341b116a4b6daddd7482a99d3ab702f4df2f5.

The complete downloaded archive was independently checked again after the original successful CI: 144 native SHA-256 source receipts (54 immutable snapshot files, 54 full-policy CSVs, 9 coupled solver CSVs and 27 physical solver JSONL streams). All were intact. A complete 54 * 60 * 10 = 32,400 method/frame AABB pair-count and ordered-pair-hash comparison against actual SAP succeeded. The source also passed direct C++ small-scene exhaustive pair oracles.

Exactly 54 genuinely new stepped material trajectories were benchmarked (six material families * three requested budgets * three independently varied geometry/topology seeds), 60 frames each, eight initial timing-warmup frames excluded. All used the RAPID motion regime. The methods were each timed once per new scene in randomized whole-trajectory order. These are Linux GitHub CI times, not independently repeated Apple Silicon measurements, controlled ABBA timing or publication-ready population confidence intervals.

No original Phase 18 snapshot, frozen V4 threshold or trained ML model was changed. The Linux runner actually executed TEN policies, not eleven: external Bullet DBVT and a trained model were unavailable and are not silently ranked.

## All original executed algorithms ranked — Guard/method

Values greater than one mean the method's per-scene mean query time was smaller than Global Guard in the geometric aggregate. The sum-of-scene-means ratio is NOT total end-to-end solver wall time.

| Actually executed policy | Guard/method geometric mean | Wins against Guard | Sum of scene mean time ratio |
|---|---:|---:|---:|
| guard_sap_bounded_trial (frozen V4) | 1.0552x | 25/54 | 1.1804x |
| global_guarded_exact | 1.0000x | 0/54 | 1.0000x |
| guard_sap_measured_probe | 0.9532x | 17/54 | 1.0203x |
| selective_guard_exact | 0.6603x | 5/54 | 0.8353x |
| region_fixed | 0.6467x | 12/54 | 1.0067x |
| region_hierarchical | 0.6317x | 12/54 | 0.9911x |
| region_adaptive | 0.6310x | 11/54 | 0.9877x |
| online_cost_aware | 0.6267x | 10/54 | 0.9682x |
| sap_persistent_coherent | 0.5551x | 16/54 | 0.9702x |
| sap_rebuild | 0.5307x | 14/54 | 0.9401x |

The 25/54 V4 win count is below half. A 1.0552x overall geometric mean does not establish general scene-by-scene dominance. It reflects concentrated benefits in some high-workload conditions.

## Independent rapid-motion material conditions

Each row below contains three different initial vertex geometries, triangle/connectivity variants and forcing-phase seeds. Actual primitive count N may differ from the requested budget.

| Material | Actual N | Guard/V4 geometric mean | V4 SAP frames across 180 | Trials | Switches |
|---|---:|---:|---:|---:|---:|
| cloth | 128 | 0.9897x | 0 | 0 | 0 |
| cloth | 512 | 0.9950x | 15 | 8 | 2 |
| cloth | 1058 | 1.2853x | 84 | 24 | 3 |
| cushion | 128 | 0.9711x | 0 | 0 | 0 |
| cushion | 512 | 0.9864x | 0 | 0 | 0 |
| cushion | 1083 | 0.9950x | 0 | 0 | 0 |
| mixed | 191 | 1.0050x | 0 | 0 | 0 |
| mixed | 727 | 0.9417x | 23 | 16 | 4 |
| mixed | 1382 | 1.0080x | 30 | 16 | 4 |
| soft_ball | 256 | 0.9967x | 0 | 0 | 0 |
| soft_ball | 1024 | 1.2601x | 124 | 40 | 8 |
| soft_ball | 2116 | 1.3300x | 106 | 32 | 7 |
| sponge | 216 | 0.9907x | 0 | 0 | 0 |
| sponge | 512 | 0.9886x | 0 | 0 | 0 |
| sponge | 1331 | 0.9638x | 15 | 8 | 2 |
| spring | 126 | 0.9865x | 0 | 0 | 0 |
| spring | 504 | 1.2018x | 57 | 16 | 2 |
| spring | 1008 | 1.2230x | 56 | 16 | 2 |

Overall V4 on-policy actions: Guard 2730 frames, persistent SAP 510 frames, 176 trial frames, 34 policy switches. Every one of the 3240 source AABB snapshots produced the correct SAP candidate count/hash under each of the ten executed policies.

Rapid cloth N1058 won on all three seeds: 1.2433x, 1.2798x, 1.3344x. However, all three switched at exactly frame index 32 and retained SAP on frames 32..59. This is independent NEW mesh geometry, but the SAME rapid-motion event schedule and broadly similar fixture family. The controller may therefore be reacting mostly to an event schedule; it is not proved across independent forcing waveforms or simulation engines.

Soft-ball N1024 won on all three seeds (1.2283x, 1.4767x, 1.1031x); soft-ball N2116 also won all three (1.5718x, 1.2394x, 1.2075x). These are exploratory spring-like soft surface fixtures, NOT measured physical pressure/constitutive models.

Spring N504 and N1008 won only two of the three seeds each; the third seed was slightly worse than Guard and never adopted SAP. Mixed N727 was slower overall (0.9417x), sponge N1331 slower (0.9638x). Cushion showed no meaningful improvement. Thus important negative cases remain alongside positive ones.

Excluding the previously identified cloth N1058 positive condition leaves 51 scenes at 1.0430x Guard/V4 geometric mean. This post-hoc sensitivity does not establish population generalization or significance.

## Nine actual coupled collision solver scenarios — DIFFERENT evidence

Three independently seeded actual 13x13 two-sheet cloth meshes, varying vertex positions, physical triangle connectivity, structural spring edges and anchor layouts, each ran free, pull and fold in the original XPBD-style collision solver (60 frames * five solver iterations). Each was independently stepped under fresh SAP, Global Guard and frozen V4. All three policies returned the same physical particle positions within 1e-7, the same discrete narrow-phase test counts and contact response counts. Source mesh triangulation/hashes and per-frame JSONL geometry were checked.

The nine SAP-reference runs totaled 5,769,002 precise collision tests and 81,390 sphere-contact response events, counted across solver iterations. This does NOT mean 81,390 unique physical objects collided.

Two important NEGATIVE findings: there were zero nonlocal same-cloth self-contact responses during the naturally driven free/pull/fold scenarios; and frozen V4 chose Guard in all 2700 coupled solver queries, with zero actual SAP trials, switches or SAP frames. Consequently, no adaptive physical solver speedup has been demonstrated, notwithstanding the separate AABB-only gains. Sequential one-shot coupled step timings have order/cache/thermal confounds.

## Separate positive controlled self-contact diagnostic

An additional native C++ test uses a test-only patch displacement to construct a real, intentionally overlapping distant patch on each independently seeded cloth mesh, then runs a precise SAP-reference self-contact pass. It produced 283, 276 and 317 nonlocal self-contact corrections across the three fixtures with bounded movement. Every collision-disabled negative control produced zero such contacts.

This verifies an isolated response path under deliberate initial overlap. It does not demonstrate naturally generated self-collision, physical correctness under continuous contact detection (CCD), general fold recovery, or any V4 switching superiority.

## Research decision and next independent validation

Frozen V4 genuinely demonstrates useful adaptive discrete broad-phase performance on three rapid-motion mesh families (cloth, soft-ball, spring) while retaining exact candidates. Performance is not uniformly better, and benefits are sensitive to workload, topology, motion and scene size. A source-backed exploratory result is possible; an unconditional solver performance claim is not justified.

The next gates are genuinely different dynamic forcing regimes (impact, shear, compression, baseline, relax), physical geometry beyond a fixed structured cloth family, actual naturally occurring nonlocal cloth self-collision, independent contact/penetration/CCD truth, external Bullet where supported, and counterbalanced repeated full-solver measurement with real V4 SAP activation.

Methods and command reference: docs/KYOTO_HELDOUT_GEOMETRY_COUPLED_PROTOCOL.md. Inspect full per-scene CSVs and the native no-rerun full-policy ranking under the CI artifact named above.
