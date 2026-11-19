# Kyoto Phase 18 repeated A/A confirmation protocol

## Why we need another timing control

The full 19 November 2026 Mac replay ranked frozen V4 first on 36 naturally
stepped trajectories: Guard/V4 geometric mean 1.0306x, 21/36 wins.
The approximate 3% advantage may be of the same order as CPU/cache/frequency
noise. This ranking is **descriptive**, not a reproducible significance test.
The complete all-eleven-method ranking and artificial-control evidence are
preserved in CBP_PHASE18_MAC_ACTUAL_2026-11-19.md.

Do not introduce another selector yet. Instead, repeat on-policy timings
of the exact same saved immutable AABB sequences and measure independent
identical-code A/A variability.

## Mechanism

The optional native --aa-control flag adds one separately instantiated
Global Guard and one separately instantiated frozen V4 to the same method
tournament. They have their own persistent caches, EWMA and switching state,
just like the originals. Only their CSV names differ:
global_guarded_exact_aa and guard_sap_bounded_trial_aa.

Every other original algorithm still executes on the same saved snapshot.
The original default CLI is unchanged; without --aa-control, no clones run.
The entire algorithm list is randomly permuted for each new replay based
on an independent, repeatable SHA-256-derived execution-order seed.

The script recheck_kyoto_phase18_aa.py checks BOTH the stored original
snapshot and original replay CSV SHA-256 values. Every policy's candidate
pair count/hash must match the SAP oracle frame-by-frame, and the new
SAP sequence must match the original Mac SAP count/hash receipts.
No scene is re-exported, no physics is resimulated, and no trained ML model
is invented.

By default, only natural scenes are included. The original artificial
rigid-translation diagnostics can be selected only via
--include-diagnostic, and any resulting scores are in separate cohorts.

## Run on the existing Mac outputs

First pull and rebuild the updated native replay:

    cd ~/Desktop/Programming/kyoto-common-contact
    git fetch origin
    git switch research/kyoto-decision-coverage
    git pull --ff-only origin research/kyoto-decision-coverage
    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_replay

Full natural cohort, 36 saved snapshots times four randomized orders:

    python3 scripts/recheck_kyoto_phase18_aa.py --results output/kyoto-decision-mac-first --replayer build/kyoto-final-cpu/ccp_material_replay --output output/kyoto-phase18-aa-mac --repeats 4
    open output/kyoto-phase18-aa-mac/AA_RECHECK.md

Optional focused exploratory replay of the rapid cloth N=1058 condition,
all three forcing-phase values, six randomized orders each:

    python3 scripts/recheck_kyoto_phase18_aa.py --results output/kyoto-decision-mac-first --replayer build/kyoto-final-cpu/ccp_material_replay --output output/kyoto-phase18-aa-rapid-cloth --repeats 6 --materials cloth --regimes rapid --min-n 1000
    open output/kyoto-phase18-aa-rapid-cloth/AA_RECHECK.md

Use a fresh output directory for each study; the scripts refuse to
overwrite evidence. The focused condition is a previously identified
positive case and must not replace the full-cohort evidence.

## Interpretation

Guard A/B is the ratio of measured runtimes of two IDENTICAL independent
Global Guard instances processing one saved trajectory. If it deviates from
1.0, order/temperature/cache/scheduler effects have produced measurable
within-session differences even without a different algorithm.

V4 A/B measures the same control for frozen V4. Its runtime or chosen
backend can differ because the controller's switching threshold depends
on observed elapsed times. The report explicitly counts frames where
the independent V4 copies chose different Guard/SAP actions.

Guard A/V4 A and Guard B/V4 B are repeated on-policy performance contrasts.
All controller, trial and pair-output costs remain in the timed method.
Report geometric means over complete trajectories and both copies
descriptively; do not count adjacent frames as independent samples.

The report also preserves median and p95 of ABSOLUTE log(A/B) ratios as
empirical variability indicators, not statistical confidence limits.
Even a Guard/V4 speedup that exceeds the observed A/A noise is not
automatically a population-level confidence claim.

The three phase-seed values from the original fixture vary forcing phase
through seed % 101. Geometry and topology are NOT independently randomized.
Likewise, four replays of the same snapshot are timing replicates, not four
new physical scenes.

Limitations: exact current AABB pairs only, not CCD, narrow phase,
physically correct mesh contact or complete solver timing. Per-policy
whole-trajectory randomized ordering reduces but does not eliminate
frequency/thermal confounds. A more carefully balanced, interleaved
and independently generated held-out workload will be needed for a paper.

No frozen V4 thresholds are changed. No production method is deleted.

## Measurement correction after the first 144-run Mac A/A experiment

The first Mac 144-run experiment successfully executed independent
Global Guard and V4 instances but accidentally labeled the first timed
copy A and second timed copy B AFTER shuffling the method Kind list.
Thus A ALWAYS executed before B for both algorithms; a systematic
early/late order difference could affect the A/B geomeans and
noise-floor summaries. The original results MUST be reported with
this limitation. They are not balanced A/B identity-order trials.

The C++ optional A/A mode is now fixed: copy identities are assigned
BEFORE random schedule shuffling, so either copy can appear first.
It does NOT change underlying algorithms or historical default runs.
The updated runner requires the new capability marker
aa_labels_shuffled_independently. Run new measurements in a distinct
destination and never overwrite the original Mac evidence.

The complete original Mac A/A results are preserved in
[CBP_PHASE18_MAC_AA_RESULTS_2026-11-19.md](CBP_PHASE18_MAC_AA_RESULTS_2026-11-19.md).

The no-new-timing source reviewer
scripts/analyze_kyoto_phase18_aa_robustness.py accepts the old
AA_RECHECK.json, checks the old CSVs and reports the historical
A/B label bias directly from recorded method execution slots.
Its fixed-scene bootstrap envelope is merely a resampling
sensitivity analysis over the **same** timing repetitions, not a
population confidence interval.

