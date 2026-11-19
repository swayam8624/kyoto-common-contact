# RBC Stage 2 — independently verified 54 material / 9 coupled native evidence

Date: 19 November 2026.

Evidence: GitHub Actions run 44545519873, commit b8b12d305292af2bc0f0e2bdaec80c7d0244cb34.
Native artifact: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44545519873/artifacts/14539627517
ZIP SHA-256: 9b8438c969d2c6c18ab1c73b0759febcb2366257267edbd5a17c083911b2cbc2

## Independent verification of original source material

The complete ~34 MB compressed artifact was downloaded, opened and
independently checked against original RBC_RESULTS.json receipts.
All 54 immutable KYOTOM01 original snapshot SHA-256 values matched.
All 54 original full-pair native replay CSV SHA-256 values matched.
All nine real coupled-solver CSV SHA-256 values matched.
There were ZERO mismatching, missing or overwritten original files.

The native experiment actually ran SIX full-pair CPU methods:
Guard fixed, original frozen V4, rebuilt SAP, persistent SAP,
node-based RBC indexed repair, and contiguous-vector RBC flat repair.
Every native full current pair set was checked against independently
computed exact SAP IDs on all 54 x 60 original material frames.
Warmup=8 and timing used the remaining 52 original frames per scene.
The original generated Markdown accidentally wrote "five methods"
in one material description. Its native CSV/JSON and actual
scoring table correctly list SIX methods; this report fixes only
the explanatory wording and leaves the original evidence untouched.

## Full-pair material timing: all 54 scenes

Metric: Guard/method ratio of per-scene mean native query times;
geometric mean across all 54 independent geometry/topology variants.
A ratio above 1 means method faster than Guard under this metric.

| Method | Guard/method geomean | Wins vs Guard | Guard/method sum-of-scene-means |
|---|---:|---:|---:|
| Fixed Guard | 1.00000x | reference | 1.00000x |
| Frozen V4 | 1.08863x | 40/54 | 1.18711x |
| RBC flat batched repair | 0.87099x | 29/54 | 1.14297x |
| Original node RBC index | 0.35631x | 0/54 | 0.34832x |
| Persistent SAP | 0.54385x | 14/54 | 0.96060x |
| Rebuilt SAP | 0.52090x | 13/54 | not independently computed here |

This means ORIGINAL indexed RBC is a serious negative result,
NOT a fastest broad phase. Flat RBC is a substantive improvement
over the pointer-heavy negative control, but still loses on
equal-scene geometric-mean performance to Guard and frozen V4.
The distinct ratio-of-sums is weighted toward expensive scenes
and is NOT full application process wall time.

## Per-material results for flat RBC, three sizes x three seeds each

| Family | Guard/flat geomean | Flat wins |
|---|---:|---:|
| Cloth | 0.86889x | 3/9 |
| Cushion | 0.35145x | 0/9 |
| Mixed | 0.95334x | 5/9 |
| Soft-ball surface | 1.12085x | 8/9 |
| Sponge | 1.01582x | 4/9 |
| Spring | 1.31717x | 9/9 |

Flat RBC is promising on spring, soft-ball and selected larger
cloth/sponge/mixed cohorts but severely regresses on cushion.
Do not suppress the cushion failures or tune on inspected seeds
then claim out-of-sample performance.

## All 18 three-seed material-size cells

| Family | Actual N | Guard/indexed | Guard/flat | Guard/V4 | Flat wins |
|---|---:|---:|---:|---:|---:|
| cloth | 128 | .33492x | .65446x | 1.10095x | 0/3 |
| cloth | 512 | .38077x | .90143x | .99749x | 0/3 |
| cloth | 1058 | .39145x | 1.11192x | 1.27124x | 3/3 |
| cushion | 128 | .19480x | .30941x | 1.06006x | 0/3 |
| cushion | 512 | .11192x | .34434x | 1.00889x | 0/3 |
| cushion | 1083 | .08927x | .40746x | 1.02719x | 0/3 |
| mixed | 191 | .38848x | .74215x | 1.04513x | 0/3 |
| mixed | 727 | .47616x | 1.02075x | .95420x | 2/3 |
| mixed | 1382 | .44405x | 1.14374x | .99883x | 3/3 |
| soft_ball | 256 | .45571x | 1.05319x | 1.02337x | 2/3 |
| soft_ball | 1024 | .36643x | 1.17651x | 1.23236x | 3/3 |
| soft_ball | 2116 | .28375x | 1.13643x | 1.37557x | 3/3 |
| sponge | 216 | .43226x | .79683x | 1.03708x | 0/3 |
| sponge | 512 | .50085x | .99532x | .99257x | 1/3 |
| sponge | 1331 | .41840x | 1.32167x | .93925x | 3/3 |
| spring | 126 | .75364x | 1.12206x | 1.18696x | 3/3 |
| spring | 504 | .66383x | 1.40934x | 1.23930x | 3/3 |
| spring | 1008 | .50004x | 1.44510x | 1.22348x | 3/3 |

## Real physically coupled solver time, not just broad phase

NINE scenarios: free, pull and fold across THREE independently
perturbed 2-cloth / 2-sphere configurations, each 60 physical
frames and FIVE actual solver collision queries per frame.
FIVE independently advanced candidate policies: SAP, Guard,
frozen V4, original RBC indexed, new RBC flat.

- Exact SAP reference particle-state equality was verified
  for the other four policies on all nine native scenarios.
- The demanding strict per-query AABB audit was separately
  tested for 16 frames; it was DISABLED for the nine timing
  sessions to avoid poisoning RBC timing with extra SAP work.
- The solver recorded 81,390 real sphere-contact corrections.
- Naturally induced nonlocal same-sheet self-contact corrections:
  ZERO. The artificial nonlocal overlap test is a different
  regression, and is NOT a naturally folding solver result.
- Total source 9 x 60 = 540 physical frames, five independent
  policy trajectories per case.

| Scenario | Seeds G/T | Guard/indexed full-step | Guard/flat full-step |
|---|---|---:|---:|
| free | 73013/86011 | .7696x | 1.0421x |
| pull | 73013/86011 | .7562x | 1.0076x |
| fold | 73013/86011 | .7390x | 1.0347x |
| free | 74017/86019 | .7552x | 1.0201x |
| pull | 74017/86019 | .7456x | .9966x |
| fold | 74017/86019 | .7557x | 1.0480x |
| free | 75029/86029 | .7798x | 1.0259x |
| pull | 75029/86029 | .7574x | 1.0288x |
| fold | 75029/86029 | .7574x | 1.0363x |

Guard/flat full-step geometric mean = 1.02656x, with
8/9 scenes slightly better than Guard; range .99665x
to 1.04800x. Guard/original indexed full-step
geomean = .75734x, 0/9 wins. Guard/frozen V4
full-step geomean approximately .99864x.

**This is the first measured, but SMALL and NOISY, full-step
win for flat RBC on these particular physical scenes.**
No repeated independent A/A, balanced ABBA, confidence
interval, external independent physics source, certified
CCD or penetration oracle has been provided. It is
NOT publication-level end-to-end superiority.

## Research decision and follow-up gates

1. KEEP frozen V4, both RBC algorithms, all negative
   conditions and original reproducible source bytes.
2. Diagnose node-heavy repairs and cushion all-pair
   overhead; compare against tuned fixed-margin Guard
   on a separate development source set.
3. Run new repeated TEN-method full-pair sensitivity
   study with two Guard A/A copies and identical
   .045/.10/.20 skin budgets. This is now implemented
   separately as ccp_rbc_material_replay and
   scripts/run_rbc_controlled_replays.py; it is not
   retroactively part of the 54-scene results.
4. Create a contact-rich natural SAME-SHEET nonlocal
   fold with independent solver penetration/CCD
   validation and genuinely dense contact.
5. Compare to actual IPC Toolkit / spatial hash /
   refit BVH implementations with identical
   narrow-phase semantics and full-stream output.
6. Present full solver step breakdowns and genuine
   time-aligned mesh visuals; exact broad-phase
   policies should yield IDENTICAL mesh positions
   unless solver fidelity changes independently.
7. Never claim a static geometric-mean material
   replay as measured full-solver performance,
   statistical significance or original novelty.

Scientific verdict: preserve Stage-2 RBC FLAT as a
candidate for honest paid-cost selection; it has
some expensive-source benefits and a small initial
real solver advantage, but is not universally faster.
Its negative controls are essential evidence.
