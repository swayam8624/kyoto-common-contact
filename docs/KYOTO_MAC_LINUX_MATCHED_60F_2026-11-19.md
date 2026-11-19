# Kyoto frozen V4 — matched 60-frame Apple Silicon and Linux evidence (2026-11-19)

Status: source-grounded **cross-platform aggregate comparison**, NOT a statistically
controlled or end-to-end physics benchmark. Frozen V4 and all original results
remain unchanged.

## Independent sources and scope

Mac source: researcher-supplied terminal transcription of the completed
output/kyoto-mac-matched-60f experiment, including every 54 HELDOUT line, all
nine COUPLED pass lines, and the full derived
full-method-scores/HELDOUT_ALL_METHODS.md table.

Linux source: completed
[GitHub Actions run 44468375567](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44468375567),
raw artifact kyoto-heldout-geometry-contact-evidence. Its archived ZIP
SHA256 is 4224c8c13123e45a3bccb9a75cd341b116a4b6daddd7482a99d3ab702f4df2f5.
All 144 Linux source-file receipts and its 32,400 method/frame SAP
fingerprints were independently checked previously. The Mac terminal
reported that its original local verifier passed the corresponding
checks, but the original Mac CSV/JSON/KYOTOM01 bytes were NOT uploaded.

Both executions requested SIX material families, RAPID motion, budgets
128/512/1024, three geometry/topology/motion seed triplets and 60
AABB frames with warmup=8. The resulting 54 natural material scenes
are **matched by procedural specification**, not yet independently
proven bit-identical between operating systems. The separate coupled
contact solver also passed nine 60-frame cases on the Mac; this is
physics state parity versus its own SAP reference, NOT an established
adaptive physics speedup.

Mac ran 11 original CPU broad phases, including Bullet DBVT. Linux
ran 10, without Bullet, and neither ran optional trained ML. Thus
even though both replay schedulers use deterministic order seeds,
the extra Mac policy changes the *set being shuffled* and potentially
the relative timing/thermal order; it is not a rigorously balanced
same-policy-slot cross-platform comparison.

## Global quantitative outcomes

| Observed metric | Mac Apple Silicon | Linux GitHub CI |
|---|---:|---:|
| Procedurally matched material trajectories | 54 | 54 |
| AABB source frames per trajectory | 60 | 60 |
| Timed frames after warmup | 52 | 52 |
| Actually executed CPU policies | 11 | 10 |
| Total policy/frame rows | 35,640 | 32,400 |
| Guard/V4 geomean of per-scene average query ratios | **1.0577x** | **1.0552x** |
| V4 trajectories faster than Guard | **39/54** | **25/54** |
| Guard/V4 sum-of-scene-means ratio (different weighting) | 1.1689x | 1.1804x |
| Guard frames selected by frozen V4 | 2,730 | 2,730 |
| SAP frames actually selected by V4 | 510 | 510 |
| Of those, bounded SAP trial frames | 176 | 176 |
| Non-trial/committed SAP query frames | 334 | 334 |
| Observed policy action changes | 34 | 34 |

The Mac and Linux overall geometric ratios are close (about
1.0577 vs 1.0552). This is **descriptive reproducibility of the
aggregate workload-level ratio**, NOT a platform-independent
speedup confidence interval or proof of stable unseen-workload gains.
In particular, 39 versus 25 per-scene wins show that smaller
individual differences change with execution environment.

The 18/18 procedure-matched material-regime/size cells have
EXACTLY the same aggregated V4 Guard/SAP/trial/switch COUNTS.
That is stronger evidence for repeatable controller action
**counts** than the overall timed ratios alone. Mac per-frame
action traces are not available here; identical aggregate counts
do NOT prove every per-seed or per-frame action was identical.
The policy controls itself use observed CPU timings, so different
schedules could offset within a group while producing the same
totals. The source snapshot byte hashes must still be compared
to establish cross-platform binary-input identity.

## All 18 directly matched rapid conditions

Guard/V4 >1 denotes V4 smaller per-scene mean AABB query time;
each condition is the geometric mean of three independently
seeded geometry/topology/motion variations. Mac ratios below are
reported to four decimals by the user's derived source, Linux
values likewise rounded here. SAP/trial/switch counts match
EXACTLY between platforms for EVERY condition.

| Material | Actual N | Mac Guard/V4 | Linux Guard/V4 | SAP frames (both) | Trial frames (both) | Switches (both) |
|---|---:|---:|---:|---:|---:|---:|
| cloth | 128 | 1.0313x | 0.9897x | 0 | 0 | 0 |
| cloth | 512 | 0.9573x | 0.9950x | 15 | 8 | 2 |
| cloth | 1058 | **1.2527x** | **1.2853x** | 84 | 24 | 3 |
| cushion | 128 | 0.9717x | 0.9711x | 0 | 0 | 0 |
| cushion | 512 | 1.0294x | 0.9864x | 0 | 0 | 0 |
| cushion | 1083 | 1.0119x | 0.9950x | 0 | 0 | 0 |
| mixed | 191 | 1.0242x | 1.0050x | 0 | 0 | 0 |
| mixed | 727 | 0.9719x | 0.9417x | 23 | 16 | 4 |
| mixed | 1382 | 1.0108x | 1.0080x | 30 | 16 | 4 |
| soft_ball | 256 | 1.0029x | 0.9967x | 0 | 0 | 0 |
| soft_ball | 1024 | **1.2172x** | **1.2601x** | 124 | 40 | 8 |
| soft_ball | 2116 | **1.3379x** | **1.3300x** | 106 | 32 | 7 |
| sponge | 216 | 1.0160x | 0.9907x | 0 | 0 | 0 |
| sponge | 512 | 1.0193x | 0.9886x | 0 | 0 | 0 |
| sponge | 1331 | 0.9613x | 0.9638x | 15 | 8 | 2 |
| spring | 126 | 0.9665x | 0.9865x | 0 | 0 | 0 |
| spring | 504 | **1.1934x** | **1.2018x** | 57 | 16 | 2 |
| spring | 1008 | **1.1627x** | **1.2230x** | 56 | 16 | 2 |

Both machines show the most material improvements on larger,
rapidly deforming cloth, the exploratory soft-ball SURFACE
spring mesh, and helical spring fixture. Both show worse
Guard/V4 ratios for mixed N727 and sponge N1331. Cushion and
small scenes remain close to parity, and some signs change
between platforms. This is a selective workload effect.

Simple equal-scene-weight material-family geometric means
(computed from the three scene groups per family, each of
which itself includes three different seeds):

| Family | Mac Guard/V4 | Linux Guard/V4 |
|---|---:|---:|
| cloth | 1.0734x | 1.0817x |
| cushion | 1.0040x | 0.9841x |
| mixed | 1.0021x | 0.9844x |
| soft_ball | 1.1777x | 1.1865x |
| sponge | 0.9985x | 0.9810x |
| spring | 1.1028x | 1.1319x |

The soft-ball and spring models are experimental spring-like
surrogates, NOT calibrated soft-body mechanics. Any "physically
more correct" or "less folding" claim requires a genuinely
independent contact/penetration/CCD source that neither
candidate-replay experiment supplies.

## Why two rankings should both be retained

The Mac full 11-method report ranked frozen V4 first by
**equal-scene-weight geometric mean** against Guard,
1.0577x and 39/54 wins. By a different quantity, the
sum of per-scene means, OnlineCostAware reached 1.3021x,
SelectiveGuard 1.2666x, regional fixed 1.2553x,
and frozen V4 only 1.1689x. The latter *ratios of sums*
give high-cost scenes greater absolute weighting and
are NOT a replacement for global elapsed process runtime.
Therefore **there is no uniquely winning algorithm across
all reasonable workload-weighting objectives**.

On the Linux ten-method test, frozen V4 ranked first by
geomean, but many individual trajectories did not beat
Guard; the earlier 25/54 win count demonstrates this.
Bullet DBVT ranked last on the Mac's exact full-output
CPU broad-phase benchmark (Guard/Bullet 0.3046x), but
these data alone do not establish universal ranking
against all possible Bullet pipelines or collision APIs.

## Limitations and next experiment, not retroactive repairs

- The Mac source is a derived original analyzer Markdown plus
  terminal output, not an independently re-read binary/CSV
  archive. Native full 54-scene source hashes and per-frame
  V4 decision sequences should be inspected to confirm
  actual equal inputs/actions between machines.
- Randomized WHOLE-POLICY trajectory order is not balanced
  independent A/A or ABBA policy copy timing. Mac included
  Bullet, Linux did not. Platform differences in CPU
  clocks, cache, thermals, background scheduling, compiler
  and precision can alter both time and policy switching.
- These ratios measure **full AABB candidate generation**,
  including actual current-frame pair filtering, not
  full collision detection, self-contact solver quality
  or end-to-end whole-physics frame time.
- The actual coupled contact solver correctly generated
  sphere contact responses and matched SAP state but
  did NOT exercise sustained V4 SAP operation in the
  previous Linux solver runs. Naturally driven same-cloth
  self-contact counts were zero; the separate deliberate
  overlap test did produce bounded self corrections.
- The variant seeds vary initial geometry/mesh connectivity,
  but all 54 conditions use the same *rapid* motion
  regime family. This is not an independent distribution
  of fully different physical applications or
  constitutive materials.
- No independent replicate timing on each condition,
  no population CI, no statistical superiority proof,
  no calibrated FEM/MPM, and no continuous collision
  detection oracle is provided.

**Scientific decision:** frozen V4 is a credible exact-candidate,
sometimes faster online broad phase on PARTICULAR rapid material
fixture families, with strikingly consistent aggregate action counts
on the two tested machines and approximately 1.056x Guard-relative
geometric ratio. The evidence does not justify calling V4 a
general fastest broad phase or a physics solver improvement.
Protect both originals, do NOT retune frozen V4 on these
now-inspected seeds. Prefer independently new motion regimes
and counterbalanced Mac policy timing A/A with an inspectable
per-frame source trace before any stronger claim.

Related internal documentation:
- docs/KYOTO_HELDOUT_54_SCENE_NATIVE_RESULT_2026-11-19.md
- docs/KYOTO_HELDOUT_54_SCENE_MAC_FIRST_2026-11-19.md
- docs/KYOTO_HELDOUT_GEOMETRY_COUPLED_PROTOCOL.md
