# Kyoto / RBC - five-run finalization contract (19 November 2026)

## Current run budget: 1 / 5 completed; 4 / 5 remain

**Run 1 completed on the user's Mac on 19 November 2026.** The isolated
11-policy repeated A/A runner compiled, verified native exact-pair identities
across the same 36 development sources, and reported paired
Frozen-V4 / RBC-Flat query geometric mean **1.05243x**, with Flat winning
**22/36** scenes. Printed Guard A/A ratios range **0.8548–1.1561x**
and warn against inferring statistical significance.

Source evidence note and all 36 rounded A/A transcript ratios:
[Run 1 log interpretation](RUN_01_REPEATABILITY_MAC_2026-11-19.md).
The original raw Mac Run-1 CSV/JSON is not present in this research
documentation and must be archived before publication.

**Next slot is Run 2; exactly four further native Mac experiment batches
remain.** Paper/slide regeneration is not a new native run.

## Decision: stop architecture sprawl, freeze the target and the budget

**Maximum additional native Mac experiment batches: FIVE.** No more "one
small rerun", no extra unreported reruns, no new algorithm families, and no
midstream retuning on held-out data. Regenerating paper/slides from existing
results is not a native experiment. A failed launch counts as a run unless it
fails in the non-compute preflight. Preserve all negative outcomes.

The original research charter asks whether **certified, solver-contracted
candidate reuse** saves end-to-end cost on real deformable contact while
preserving the exact candidate contract. The current RBC Stage-2 materials
measure *current-frame, exact, full AABB pair enumeration* only. A successful
AABB experiment is one part of the research question, not the answer.

**Primary publishable claim to test (do NOT assert yet):**

> Under exactly matched candidate-pair semantics and unchanged coupled
> contact physics, a contiguous/flat certified frontier may reduce total
> solver step cost on selected coherent deformable workloads compared with
> frozen V4 and fixed Guard, even if it does not win query-time GM across
> all materials.

This is a regime-qualified empirical hypothesis, NOT a universal
algorithmic novelty claim, CCD proof, broad application speedup, or calibrated
material-physics result. If falsified, the final paper reports a negative
benchmark and a defensible engineering recommendation.

## Evidence already captured - count zero new runs

1. **Frozen V4 historical release:** V5 864 scene-cell Guard/V4 GM 1.091216x,
   V6 independent 864 cell GM 1.083201x; query time only.
2. **RBC Stage-2 verified 54 material / 9 coupled:** Guard/V4 query GM
   1.08863x, Guard/Flat query GM 0.87099x, Guard/Flat solver-step GM
   1.02656x. Multiple source variants, but not independent balanced
   thermal sessions; no naturally induced self-contact.
3. **Mac 36 material / 3 coupled pilot:** Guard/V4 query GM 1.1130x,
   Guard/Flat query GM 1.0580x, query-only. Coupled Full step times
   (ms) free: V4 4.221, Flat 3.628; pull 4.765, 4.351;
   fold 4.122, 3.568. These are one seed and one timing session.
4. **Rounded prepared-fold geometry validation:** endpoint plus linearly
   sampled triangle piercing tests passed on three 80-frame reference
   source variations. Not natural folding and not swept CCD.

Do not average the different machine/cohort score sheets into a single
numerical "benchmark result". Preserve each source hash and hostname.

## Five Mac-run schedule - protocol-locked

| Slot | Research gate | Main output | Failure outcome |
|---|---|---|---|
| **1** | Controlled within-scene repeated exact-pair query comparison, Guard A/A copies, V4, RBC Flat, SAP and indexed negative control, with 4+ repetitions of SAME immutable pilot sources; cold/warmup and execution order fully logged | A/A ratios, paired per-scene query-time ratios, per-material matrix and raw traces | If A/A noise is not acceptably small relative to the claimed speedup: no query superiority claim |
| **2** | Independent **held-out** geometry/motion seeds (predeclared), six materials, at least three scales and rapid/shear/impact where supported; all original candidates verified against fresh SAP | Native full-pair held-out source manifest, per-regime speedups, degradation cases | If Flat loses GM, report conditional rather than global win; never modify source/threshold after inspection |
| **3** | Paired whole-solver runs: free/pull/fold with multiple predeclared seeds; distinguish strict-oracle correctness session from timing session and avoid charging audit overhead to one arm | Whole-step, broad/narrow timing, particle-state equality, full contact/candidate counters, A/A noise control if supported | If solver benefit within timing noise: no end-to-end acceleration claim |
| **4** | Physical stress/visual proof on true native trajectories: zero/positive/negative geometry states, natural contact attempts, more than one scenario, independent endpoint/subframe audits, source-backed renders | Raw mesh JSONLs, geometry audit JSON, time-aligned 3D GIF/HTML, actual contact plots | If only prepared fold is clean, say so; label natural stress failure. Never beautify geometry to hide violations |
| **5** | Final confirmation on LOCKED untouched challenge set (only after design decisions freeze), plus package integrity/checks + final paper/deck export | Frozen metric/negative-result tables, method/code hashes, PPTX/PDF and initial research paper with citations | If results falsify hypothesis, freeze a qualified/negative paper and retain validated V4 as production |

**Run** means one user-approved native Mac experiment batch/command.
Compilation, static analysis, report regeneration and GitHub CI can happen
without consuming a new native experiment, but DO NOT trigger more Mac
native benchmarking outside these five slots.

## RUN 1 / 5 - exact command (native runner CI preflight passed)

The original 36-scene Mac source cohort already exists. Use it as a
DEVELOPMENT/repeatability cohort; do not call it untouched holdout.
The isolated native ccp_rbc_final_repeated executes 11 exact full-pair
methods, including two Guard A/A copies, Frozen V4 and RBC Flat, with
four repetitions and alternating forward/reverse whole-trajectory orders.
This is a source-matched repeated query experiment, not a balanced
independent thermal ABBA study or a whole-solver timing experiment.

~~~bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/reach-budget-calculus
git pull --ff-only origin research/reach-budget-calculus

cmake --preset kyoto-final-cpu
cmake --build build/kyoto-final-cpu --parallel 4 \
  --target ccp_rbc_final_repeated

python3 -u scripts/run_rbc_final_repeated.py \
  --replayer build/kyoto-final-cpu/ccp_rbc_final_repeated \
  --manifest output/rbc-observatory-native-pilot-01/RBC_RESULTS.json \
  --output output/rbc-final-run01-controlled-01 \
  --repeats 4 --warmup 8 \
  | tee output/rbc-final-run01-controlled-01.console.txt
~~~

Return RBC_FINAL_REPEATED_RESULTS.md and RBC_FINAL_REPEATED_RESULTS.json
from that new evidence directory. Do not re-run merely to replace a
negative score. The source-backed native CI smoke already passed:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44574344244

No subsequent native Mac experiment should be launched until the
Run-1 A/A and paired V4/Flat results have been assessed.

## Prior registration and fairness

- Snapshot the exact branch/commit, source SHA256, compiler, build mode,
  CPU/OS, per-process temperature/thermal notes when obtainable, thread
  count, wall-clock start/end, process stdout/stderr, and exact arguments.
- Never tune after reading the Slot-5 challenge. Seeds must be fixed in
  the protocol before Slot-2 and Slot-5 and must not overlap development.
- Match full sorted pair IDs and masks, not counts alone. Keep an
  independent small-scene exhaustive oracle.
- Separate current AABB versus reach-inflated versus swept contract,
  and **never** compare routines of different contracts as equal.
- Equal exact candidates and initial solver states imply identical
  physics under the same contact solver: broad-phase improvements CANNOT
  be credited with fewer wrinkles or better folding.
- Report full per-scene distributions, p50/p95/p99 where source permits,
  per-scene ratios, sample sizes, bootstrap percentile intervals over
  independent scenes (not pseudo-independent frames), and the A/A noise.
- Split query-only and whole-step results. Disclose material/scale
  regressions, per-scene exceptions, and CPU-vs-GPU boundaries.
- Correctness requires source hashes, reference pairs, candidate sets,
  world-state parity, and independent triangle diagnostics; discrete
  frame checks are not full continuous CCD.
- Never treat "synthetic" contact source labels as calibrated material
  simulation or externally validated benchmark scenes.

## Accept/fail criteria

**Engineering freeze** (achievable even with negative speed results):
existing CI and exact-pair tests pass, all five experiment slots are
accounted for, reproducibility manifests and source hashes exist,
original V4 is unchanged, data is preserved, the final repo cleanly
builds, and the slide deck plus initial manuscript truthfully use the
frozen evidence.

**Positive acceleration claim** (conditional): paired solver step
improvement is repeatable across held-out scenes with intervals and
A/A controls that exclude noise as a plausible explanation; no oracle
misses; full-state equivalence; at least one genuine contact-rich scene.
If any condition is missing, label the finding exploratory or negative.

**Novelty claim:** withheld unless related work and independent
baselines establish a precise *new* contribution over temporal coherence,
SAP, DBVT, IPC/broad-phase/reach certificates. "Adaptive, certified"
alone is not novelty.

## What must exist at the end

- FROZEN_FINAL_RESULT.md with executive findings and all negative cases.
- MACHINE_AND_SOURCE_MANIFEST.json, RAW_SCORE_CSV, SUMMARY_TABLES.csv,
  A_A_NOISE_REPORT, and all correctness gate receipts.
- Final professor-ready **PPTX** (slide deck with figures, native source
  screenshot/video links, actual performance and quality diagnostics).
- Initial technical **paper** (PDF + editable LaTeX) with abstract, problem
  formulation, exact geometric contract, algorithm, reproducible method,
  prior art, full results, negative controls, limitations, and bibliography.
- Code archived and version-tagged; no requirement to invent a
  publishable novelty claim or new physics engine in five experiments.

## Stop/route logic

If only V4 passes statistical/reproducibility gates, ship V4 and submit
the manuscript as a comparative/negative engineering study, not
"RBC beats SOTA". If Flat has repeatable conditional improvements, its
paper claim is regime-specific. If physics remains only prepared-fold,
we explicitly exclude any claim of superior collision response or CCD.
**The five-run budget is a ceiling, not a mandate to waste all five.**

Paper/deck preliminary figures are based on the already recorded
36/54/9 cohorts. They are DRAFT evidence until the final source receipts,
held-out outcomes and external baselines are reconciled.
