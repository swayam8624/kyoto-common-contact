# Kyoto decision coverage after the Phase 17 Mac audit

## What the actual 19 November Mac report establishes

The first experiment contains 36 trajectories x 24 frames = 864 executed V4
frames. Every actual scene has N in {126,128,191,216,256}, below the
frozen 384-AABB minimum for starting a V4 SAP trial. Actual choices:
864 Guard, 0 SAP, 0 trials, 0 backend switches. Warmup-excluded
trajectory-geomean Guard/V4 = 0.4475x. This is an important **small-scene
negative result for V4**, but does not measure adaptive switching.

Do not reinterpret the learned C++ ABI smoke test as a trained ML run:
no `.mlpc` weights were supplied to the 36 material replay experiments.
Bullet DBVT, by contrast, was compiled and executed on the user's Mac.

## New non-destructive control

The follow-up implementation lives on branch
`research/kyoto-decision-coverage`; frozen V4 production code is unchanged.

1. The original complete-pair material replayer now supports the optional
   argument `--order-seed N`, deterministically shuffling full-output
   method execution order. The reference SAP pair list is precomputed in a
   separate untimed verification pass before shuffled runs. The CSV carries
   the actual execution slot and shuffle seed for inspection.
2. The new `scripts/run_kyoto_decision_coverage.py` exports actual original
   mass-spring material snapshots at requested budgets 512 and 1024, with
   3 different seeds by default. It reads the actual KYOTOM01 header and
   FAILS if the generated object count is below 384.
3. It includes a separate **artificial rigid-translation diagnostic**.
   Alternating .30-world-unit whole-scene translations stress Guard's
   .045-world-unit reuse envelope. Relative body geometry is preserved.
   It is not a physically simulated impact and cannot be pooled with
   the natural material-scene performance measurements.
4. Every output is checked against framewise SAP pair count/hash using
   the original scorer. The native small-scene oracle and multiple seeded
   execution orders have independent regression tests.
5. Actual V4 Guard/SAP action counts, trial frames and switches are
   collected from the emitted CSV; NO `N>=384` scene is counted as a
   successful switch unless V4 actually selected SAP.
6. Outputs are immutable, input/output file SHA-256 recorded, and no
   on-policy controller threshold is retuned from previous benchmarks.

## Reproduce on the Mac (new output, nothing overwritten)

    cd ~/Desktop/Programming/kyoto-common-contact
    git fetch origin
    git switch research/kyoto-decision-coverage
    git pull --ff-only origin research/kyoto-decision-coverage
    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_export ccp_material_replay
    python3 scripts/run_kyoto_decision_coverage.py --exporter build/kyoto-final-cpu/ccp_material_export --replayer build/kyoto-final-cpu/ccp_material_replay --output output/kyoto-decision-mac-first

Review `DECISION_COVERAGE.md`, `DECISION_COVERAGE.json` and
per-trajectory full-output CSVs. The default matrix is 36 physically
stepped material trajectories plus three rigid-translation diagnostics:
cloth, sponge and mixed at requested budgets 512 and 1024, baseline and
rapid regimes, three fixed seeds, 60 frames per trajectory. This is
bounded and does NOT rerun the million-AABB Phase 16 pressure tests.

For a smaller confirmation run, pass e.g.:

    --budgets 512 --materials cloth sponge --regimes rapid --seeds 3203 --frames 36 --warmup 4

Use `--model /path/to/original-trained-controller.mlpc` only if you have
the genuine trained weights and their held-out training provenance. A
manufactured constant-cost smoke model is not a real ML comparison.

## Scientific reading of the evidence

The question is whether an on-policy controller finds genuinely
expensive Guard rebuild streaks and exploits SAP when profitable, not
whether a fixed size threshold is crossed. The condition also requires
three successive costly rebuilds with individual elapsed >=.25 ms and
guard EWMA >=.35 ms, followed by a trial evaluated with charged costs.

The initial Linux 512-AABB integration control produced 48 natural and
24 artificially translated V4 frames. It passed exactness and randomized
execution-order checks, but **all 72 selected Guard and none selected SAP**.
That is a valid negative *activation* result, not a correctness failure.

The 2048-AABB control was then actually executed on Linux and passed full
candidate-set checks. In its 60-frame **natural rapid cloth** trajectory,
V4 chose Guard for all 60 frames (no trial). On its distinct 60-frame
**rigid-translation diagnostic**, V4 chose Guard for 3 frames, SAP for
57 frames, recorded 8 SAP trial frames, and recorded 1 switch.
This is evidence that the original switching mechanism is reachable on an
adversarial fixture, NOT proof that it improves natural cloth simulation.

A separate 512-AABB integration run switched once on the diagnostic (3
Guard and 21 SAP frames), while an earlier Linux 512-AABB run did not
switch at all. The 512 result is explicitly **timing/host-sensitive**;
neither result should be generalized without per-machine replications.
See the [source-bound full CI job](https://github.com/swayam8624/kyoto-common-contact/actions/runs/44432043265).

Do not equate full-output AABB candidate time with narrow phase,
continuous collision detection, contact solver, mesh quality or the
large-scale grid digest queries. For a research paper, preserve these
measurement boundaries, use repeated independent seeds, report
trajectory-level evidence and compare external libraries on equal terms.


## Mac evidence from the completed 39-trajectory experiment

The Mac's full 36 natural + 3 artificial run is summarized in
[CBP_PHASE18_MAC_ACTUAL_2026-11-19.md](CBP_PHASE18_MAC_ACTUAL_2026-11-19.md).
Natural frames: 2,059 Guard and 101 SAP; 40 SAP trial frames and 7 actual
backend transitions. Warmup-excluded Guard/V4 trajectory-geomean:
1.030587x, with 21/36 trajectories favoring V4. The meaningful
success cases were three rapid cloth, actual N=1058, independently seeded
trajectories; two sponge trials were rejected and returned to Guard.
Artificial translation has its own separately reported, nonphysical cohort.

The earlier Mac run at N<384, where V4 chose Guard on all 864 frames,
remains an equally important negative example. The two reports answer
different questions; do not overwrite either with the other.

The first 39-run decision report retained only Guard/V4 timings despite
successfully executing 11 algorithms. The additive post-run analyzer
reuses the **original CSVs** and enforces their recorded hashes:

    cd ~/Desktop/Programming/kyoto-common-contact
    git pull --ff-only origin research/kyoto-decision-coverage
    python3 scripts/analyze_kyoto_decision_coverage.py --results output/kyoto-decision-mac-first --output output/kyoto-decision-mac-first/verified-all-methods-v1
    open output/kyoto-decision-mac-first/verified-all-methods-v1/VERIFIED_ALL_METHODS.md

The analyzer never times a backend, cannot silently pool artificial with
natural scenes, refuses evidence replacement and validates all policy
pair lists against SAP. A complete Linux integration regression with
a deliberately falsified manifest hash passed:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44434453461
