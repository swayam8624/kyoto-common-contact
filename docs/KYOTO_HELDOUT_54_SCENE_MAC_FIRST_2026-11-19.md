# Kyoto 54-scene held-out first Mac result — console-reported, 19 November 2026

## Evidence provenance

This note is derived exclusively from the author-supplied
Mac terminal transcript of a successful local execution of
scripts/run_kyoto_heldout_research.py, scripts/test_kyoto_heldout_research.py,
and scripts/analyze_kyoto_heldout_evidence.py on 19 November 2026.

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
HELDOUT_RESULTS.json and full-methods report were NOT uploaded with
the console excerpt. Consequently, the numbers here are calculated
from the 54 **rounded-to-four-decimal Guard/V4 ratios** in the
terminal excerpt, and the exact original method ranking, pair hashes,
per-frame V4 actions, source SHA256 values and original solver timings
have NOT been independently re-examined in this GitHub note.
The successful local verifier is evidence of those checks, not a
license to claim that the raw Mac evidence is archived here.

Mac CMake configuration reported external Bullet DBVT enabled.
The console did NOT display the full heldout all-method ranking; the
local file output/kyoto-heldout-mac-first/full-method-scores/
HELDOUT_ALL_METHODS.md contains that ranking. Do not copy the
Linux ten-method ranking into the Mac report, or assume an exact
Mac method count without inspecting its generated file.

The zsh "command not found: #" messages are from literal pasted
comment lines, not failed build or benchmark steps. Eight Clang
C99-extension warnings originated in third-party Homebrew Bullet
headers. Compilation and linking succeeded.

## What the Mac actually measured

The default Mac suite is THREE material families (cloth, sponge,
mixed), TWO forcing regime names (baseline, rapid), three requested
budgets (128, 512, 1024), and THREE new mesh/geometry/forcing
seed tuples = 54 trajectories. Each was stepped for 60 frames,
with the first eight excluded from the reported mean CPU query
times. The coupled solver separately ran free, pull and fold on
three independently varied two-sheet mesh variants, 60 frames
each (540 scenario frames in total, separate policy runs).

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
