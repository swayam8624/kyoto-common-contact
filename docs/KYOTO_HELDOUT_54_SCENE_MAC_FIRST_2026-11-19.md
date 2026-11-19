# Kyoto 54-scene held-out first Mac result — console-reported, 19 November 2026

## Evidence provenance

This note is derived from two user-supplied 19 November 2026
Mac artifacts: (1) the terminal transcript of successful native
run + integrity checks, and (2) the complete textual contents
of the analyzer-generated HELDOUT_ALL_METHODS.md report.
The original raw native Mac CSVs, binary snapshots and JSONL
streams remain local; they were NOT provided to the repository.

The Mac run fast-forwarded the working branch from commit 37475ea to
243e325, built all five requested targets, and printed successful
per-scene HELDOUT rows, nine COUPLED rows and final verification
messages:

- PASS: independently varied first-frame geometry AND coupled solver
  mesh adjacency, all original full-pair policy outputs, receipts
  and coupled mesh/contact parity.
- scenes: 54, coupled scenario-frames: 540.
- PASS: 54 independently seeded full original-method CSVs reverified
  with SAP, no reruns.

The complete original Mac raw CSVs, immutable snapshots,
HELDOUT_RESULTS.json and full-methods JSON were NOT uploaded.
The 54 per-scene Guard/V4 ratios in this note were calculated
from the terminal's four-decimal printed values. The 11-policy
aggregate ranking and V4 action totals below were separately
copied from the actual Markdown report subsequently provided
by the user. Neither raw per-frame original pair hashes nor
individual method timings have been independently inspected
outside the Mac. Successful local verification supports their
validity but is not an original-source re-verification here.

Mac CMake configuration reported external Bullet DBVT enabled.
The user's subsequently supplied original
output/kyoto-heldout-mac-first/full-method-scores/HELDOUT_ALL_METHODS.md
establishes that 11 algorithms actually ran on the Mac,
including real Bullet DBVT (but NOT an ML model). Do not copy
the Linux ten-method ranking into the Mac report.

The zsh "command not found: #" messages are from literal pasted
comment lines, not failed build or benchmark steps. Eight Clang
C99-extension warnings originated in third-party Homebrew Bullet
headers. Compilation and linking succeeded.

## What the Mac actually measured

The default Mac suite is THREE material families (cloth, sponge,
mixed), TWO forcing regime names (baseline, rapid), three requested
budgets (128, 512, 1024), and THREE new mesh/geometry/forcing
seed tuples = 54 trajectories. Each material trajectory was stepped for **36 frames**,
with the first eight excluded from its reported mean CPU
query time (28 timed frames per trajectory). The runner at
this source commit defaulted to --frames 36. This yields
54 x 36 = 1944 V4 backend decisions, 54 x 28 = 1512
post-warmup timing frames per method and 54 x 36 x 11 =
21384 original policy/frame records on the Mac.

The COUPLED solver separately ran free, pull and fold on
three independently varied two-sheet mesh variants for
60 frames each (540 scenario frames, per-policy independent
runs). Do not confuse these with the 36-frame AABB replays.

Guard/V4 >1 favors the frozen V4 implementation. All values
are broad-phase candidate query times, not end-to-end solver time.

**Overall 54-scene Guard/V4 geometric mean: 0.999160x**
(rounded 0.9992x), with 27 of 54 individual scene wins.
This is approximately -0.084% in reciprocal mean-time ratio, i.e.
descriptive **near-parity**, not a meaningful overall speedup.

| Subcohort | Guard/V4 geomean | Wins |
|---|---:|---:|
| All baseline (27 scenes) | 1.010115x | 16/27 |
| All rapid (27 scenes) | 0.988324x | 11/27 |
| All cloth (18 scenes) | 1.009656x | 10/18 |
| All sponge (18 scenes) | 0.986505x | 8/18 |
| All mixed (18 scenes) | 1.001458x | 9/18 |
| Budget 128 (18 scenes) | 0.989639x | 7/18 |
| Budget 512 (18 scenes) | 0.999864x | 8/18 |
| Budget 1024 (18 scenes) | 1.008063x | 12/18 |

## Complete Mac per-condition sensitivity

Each cell contains exactly three distinct new source mesh/seed
tuples, not repeated independent Mac timing runs. N is the
actual AABB primitive count, not the requested input budget.

| Material | Regime | Actual N | Guard/V4 geomean | Wins |
|---|---|---:|---:|---:|
| cloth | baseline | 128 | 1.025350x | 2/3 |
| cloth | baseline | 512 | 1.031295x | 3/3 |
| cloth | baseline | 1058 | 0.993547x | 1/3 |
| cloth | rapid | 128 | 0.914989x | 0/3 |
| cloth | rapid | 512 | 0.988712x | 1/3 |
| cloth | rapid | 1058 | 1.114584x | 3/3 |
| mixed | baseline | 191 | 0.982209x | 1/3 |
| mixed | baseline | 727 | 0.996293x | 2/3 |
| mixed | baseline | 1382 | 1.003305x | 1/3 |
| mixed | rapid | 191 | 1.004220x | 2/3 |
| mixed | rapid | 727 | 0.994298x | 0/3 |
| mixed | rapid | 1382 | 1.029025x | 3/3 |
| sponge | baseline | 216 | 1.040777x | 2/3 |
| sponge | baseline | 512 | 1.000264x | 1/3 |
| sponge | baseline | 1331 | 1.019523x | 3/3 |
| sponge | rapid | 216 | 0.975401x | 0/3 |
| sponge | rapid | 512 | 0.988952x | 1/3 |
| sponge | rapid | 1331 | 0.900259x | 1/3 |

The apparently favorable baseline cloth N128 average includes
individual ratios 0.8570x, 1.1192x and 1.1239x. The frozen V4
implementation cannot enter its SAP trial for fewer than 384
AABBs, so such disparate same-Guard-path ratios warn about
native timing noise and protocol order—not meaningful adaptive
backend selection. Policy bookkeeping and actual timing order
still differ slightly. A reliable speed claim needs balanced
copy-order time repeats on the same Mac.

## Strongest and weakest Mac cases

Rapid cloth actual N1058, the earlier Phase 18 positive case:
independent seed rows 1.0524x, 1.0472x, 1.2564x, geomean
1.114584x (three of three positive). This suggests a
workload-specific Mac benefit while highlighting that one seed
contributes a disproportionately large effect. Approximate
Guard-relative measured query time reduction in that condition:
10.3%, not a 11.5%-point reduction in wall-clock time.

Rapid sponge actual N1331:
1.0009x, 0.9867x, 0.7388x, geomean 0.900259x
(one of three positive). The 0.7388x case means V4 took
roughly 35.4% more measured mean query time than Guard in
that trajectory, not a candidate correctness failure.
This single-run timing should be independently repeated
before attributing it to a stable controller mistake.

Removing the positive rapid-cloth N1058 cell post hoc:
Guard/V4 across remaining 51 scenes = 0.992756x.
Removing negative rapid-sponge N1331 instead:
remaining 51 scene Guard/V4 = 1.005305x.
These are sensitivity calculations, NOT alternate prospective
benchmark scores or population confidence intervals.

## Why the Linux CI score must not be transplanted to this Mac run

The earlier 54-scene Linux result, in
KYOTO_HELDOUT_54_SCENE_NATIVE_RESULT_2026-11-19.md,
was a different TEST MATRIX: all SIX material families,
RAPID regime only, three budgets, three seeded physical variants;
it produced Guard/V4 geometric mean 1.0552x with 25/54
wins, and soft-ball/spring held-out positives.

The Mac 54-scene default matrix contains only cloth/sponge/mixed
but BOTH baseline and rapid regimes. It should never be
compared with the Linux 1.0552x ratio as if it were a direct
same-scene cross-platform replication. For a proper cross-machine
comparison, use identical scene lists, same binaries/build
capabilities and record precise warmup and CPU timing controls.
Even with matching procedural seeds, different hardware,
compiler, scheduling and thermal conditions can affect measured
speed and online V4 policy decisions.

This Mac excerpt does NOT expose the per-frame
Guard/SAP/trial/switch outcomes. Those are saved in the
original local full-method scores JSON and raw native CSV.
Therefore no claim about how often V4 adopted SAP on the Mac
is made here.

## Scientific decision

The full 54-scene Mac broad-phase result supports
**near overall parity**, with a conditional positive on
large rapid cloth and some serious regressions worth
controlled repeat studies. The exact candidate set and
physical-solver parity validations completed successfully,
but the console does not establish analytic penetration truth,
CCD, self-contact on naturally driven folds or end-to-end
adaptive V4 physical solver speedup.

Freeze V4. Do not tune on this held-out dataset. Preserve the
original Mac output/kyoto-heldout-mac-first folder intact.
Use the already-created all-method source report to evaluate
the actual Bullet DBVT and other native Mac methods once its
contents are accessible. Prefer targeted, balanced Mac
A/A time repeats with independent A/B identity before
shuffling, and additional forcing histories on new meshes
over further broad superiority claims.


## Complete Mac eleven-algorithm ranking from submitted derived report

The data below are the original locally generated
HELDOUT_ALL_METHODS.md contents pasted into the conversation.
Guard/method > 1 indicates the alternative has a smaller
geometric mean of per-scene average AABB query times.
These are query timings, NOT full end-to-end physics, native
solver performance or controlled population confidence intervals.

| Actually executed Mac method | Guard/method geomean | Scene wins vs Guard | Sum of scene-means ratio |
|---|---:|---:|---:|
| global_guarded_exact | 1.0000x | 0/54 | 1.0000x |
| guard_sap_bounded_trial (frozen V4) | 0.9992x | 27/54 | 0.9903x |
| selective_guard_exact | 0.8856x | 22/54 | 1.0789x |
| guard_sap_measured_probe | 0.8166x | 12/54 | 0.6412x |
| online_cost_aware | 0.7977x | 14/54 | 0.9929x |
| region_fixed | 0.7429x | 11/54 | 0.9722x |
| region_adaptive | 0.7233x | 11/54 | 0.9445x |
| region_hierarchical | 0.7224x | 11/54 | 0.9368x |
| sap_persistent_coherent | 0.4284x | 1/54 | 0.5550x |
| sap_rebuild | 0.4116x | 0/54 | 0.5341x |
| bullet_dbvt_persistent | 0.2274x | 0/54 | 0.3355x |

Global Guard ranks first by the stated geometric benchmark
summary; V4 is a near tie and wins exactly half the scenes.
Persistently rebuilt SAP and Bullet DBVT are slow on these
specific material AABB streams; they were not physically
validated beyond exact current candidate counts/hashes.
The native Bullet presence on the Mac makes this 11-policy
Mac ranking materially different from ten-policy Linux CI.

Selective Guard illustrates why metrics must remain distinct:
its geometric ratio 0.8856x means it is generally less
competitive multiplicatively across scenes, yet its
1.0789x *sum-of-scene-means* ratio means it did better
on the larger-cost scene means that dominate that separate
weighting. Neither score is total native process wall time
or a balanced-noise benchmark.

## Mac V4 on-policy actions: actual trials versus established SAP

Original submitted all-method report:
Guard frames 1895, SAP frames 49, trial frames 42,
backend action changes 10. The totals exactly reconcile
to **54 trajectories x 36 frames = 1944** decisions.

- Guard: 1895 / 1944 = 97.48%.
- SAP: 49 / 1944 = 2.52%.
- SAP trial: 42 of the 49 SAP frames.
- Non-trial SAP: **7 of 1944**. These are the only
  SAP query frames outside the bounded trial mode, and
  ALL seven fall in the rapid sponge N1331 condition.
- There were 10 changes in the currently executed
  backend action across the 54 trajectories.

| Source condition | SAP action frames | Trial frames | Non-trial SAP frames | Backend action changes |
|---|---:|---:|---:|---:|
| cloth rapid N512 | 4 | 4 | 0 | 1 |
| cloth rapid N1058 | 12 | 12 | 0 | 3 |
| mixed rapid N727 | 8 | 8 | 0 | 2 |
| mixed rapid N1382 | 10 | 10 | 0 | 2 |
| sponge rapid N1331 | 15 | 8 | 7 | 2 |
| All other 13 conditions | 0 | 0 | 0 | 0 |
| **All 18 conditions** | **49** | **42** | **7** | **10** |

**Important horizon censoring, not evidence that Mac always
rejects SAP:** frozen V4 requires as many as eight trial frames
to promote a candidate SAP choice into committed SAP mode.
The original Linux rapid cloth N1058 trial began near frame
index 32 and then continued past frame 36. The original Mac
AABB replay STOPPED after index 35. Consequently all 12
rapid-cloth Mac SAP frames were still *trials*; a 36-frame
stream cannot reveal whether sustained SAP mode would have
been accepted on later frames.

This makes the earlier interpretation "Linux V4 switched
but Mac V4 did not" methodologically premature. The
different motion/time horizon, different source cohorts,
hardware and scheduling confound that comparison.
The rapid-cloth Mac query ratio of 1.1146x is therefore
a **real reported ratio on 28 post-warmup frames**, but it
cannot be attributed to sustained adaptive SAP operation.
The trial/SAP behavior is an exploratory controller
observation, not an efficiency explanation.

The seven nontrial SAP frames in the sponge rapid N1331
condition coexist with its poor 0.9003x aggregate ratio.
Even here, the aggregate three-seed count is not sufficient
to infer which particular seed drove the seven committed
SAP frames or the 0.7388x regression. That requires
reading the original per-seed CSVs, not speculation.

## Corrected protocol for FUTURE matched runs

The research orchestrator now defaults to --frames 60,
and prints the actual material frame horizon separately
from the coupled solver frame horizon. This change
does NOT alter, overwrite or retroactively repair the
completed 36-frame Mac evidence.

An intentionally matched six-family Linux-versus-Mac
study must explicitly request the same six material
families, rapid regime, 128/512/1024 budgets, three
geometry/topology seeds, **60 frames**, and warmup=8.
In particular the 60-frame horizon is required to
observe more of V4's trial-and-adoption behavior after
the rapid-cloth transition at frame index 32.
Still use balanced method-order repeated A/A controls
before presenting cross-platform performance CIs.
