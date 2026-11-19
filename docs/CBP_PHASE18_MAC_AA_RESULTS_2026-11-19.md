# Kyoto Phase 18 Mac repeated A/A: measured results and audit corrections

Date: 19 November 2026. This note preserves the author's user-supplied
original Mac AA_RECHECK.md report and build console. The old Mac raw CSV
files remain local and were not uploaded into this GitHub branch.
No result in this note pretends to have independently run on the Mac.

## Actual 144-replay study

36 natural AABB trajectories with four randomized policy-order repeats
each, plus two independently instantiated Global Guard and frozen V4
methods per run. These are 144 timing sessions of the SAME 36 scene
snapshots, NOT 144 independent material geometries.

- Natural Guard/V4 geomean over both copies: 1.0188x.
- Prior single-run natural Guard/V4 geomean: 1.0306x.
- Guard A/B geomean: 1.0027x; V4 A/B geomean: 1.0088x.
- Median |log(Guard A/B)|: 0.02152, roughly 2.18% timing disagreement.
- P95 |log(Guard A/B)|: 0.16280, roughly 17.68% disagreement.
- V4 A/B disagreed about the chosen backend on exactly 7 out of
  144 x 60 = 8640 paired frames. V4 A SAP 411 frames, B 404 frames.
- All seven disagreement frames occur in sponge, baseline, actual
  N=1331, phase seed 3203, timing repeat 2. In that run Guard A/B
  was 0.7133x and V4 A/B was 1.4576x. This does NOT establish
  a candidate pair correctness failure.

## Complete natural condition scores

| Material | Regime | Actual N | Repeated Guard/V4 geomean | V4 A/B differing frames |
|---|---|---:|---:|---:|
| cloth | baseline | 512 | 1.0224x | 0 |
| cloth | baseline | 1058 | 0.9919x | 0 |
| cloth | rapid | 512 | 1.0020x | 0 |
| cloth | rapid | 1058 | 1.2139x | 0 |
| mixed | baseline | 727 | 1.0092x | 0 |
| mixed | baseline | 1382 | 1.0111x | 0 |
| mixed | rapid | 727 | 0.9982x | 0 |
| mixed | rapid | 1382 | 1.0004x | 0 |
| sponge | baseline | 512 | 1.0099x | 0 |
| sponge | baseline | 1331 | 0.9729x | 7 |
| sponge | rapid | 512 | 1.0105x | 0 |
| sponge | rapid | 1331 | 1.0025x | 0 |

The overall 1.0188x geometric mean across twelve equally represented
conditions drops to approximately 1.0027x when the single rapid-cloth
N=1058 condition is excluded. That is a POST-HOC sensitivity calculation,
not a silently edited benchmark score. The rapid-cloth condition itself
retains 1.2139x on the exact same phase-varied saved snapshot family,
equivalent to about 17.6% lower measured broad-phase query time.

## Important instrumentation correction after the run

An audit of the actual optional A/A C++ code found a subtle timing-order
problem. The original implementation shuffled Kind enum values and only
then assigned A/B labels to duplicate Guard and V4 instances during
execution. Therefore copy A always executed BEFORE its duplicate copy B.
The A/B labels were NOT themselves randomly permuted.

The two C++ methods and their persistent caches were genuinely
independent, and exact SAP candidate count/hash parity still passed.
However, the original A/B ratios also capture this deterministic
early-versus-late timing-order effect. They CANNOT be described as
balanced independently randomized copy-label experiments. Their
descriptive timing-disagreement value remains useful with this caveat.

The optional A/A mode has now been FIXED: identities are assigned to
two distinct scheduling records BEFORE shuffling. Thus future copies
A and B can appear in either order across repeated runs. The original
default replay without the A/A flag and the frozen V4 implementation
remain unchanged. The new C++ binary exposes capability marker
aa_labels_shuffled_independently, and the Python replay runner now
requires it. The original 144-run Mac evidence is preserved unchanged.

A new purely post-run analyzer can inspect all old 144 saved source
CSV SHA-256 receipts, recheck framewise SAP count/hash parity, audit
each A/B execution slot to warn about the original label-order bias,
compute per-condition/leave-one-out results, and identify the exact
seven per-frame V4 choice disagreements. It also calculates an
OPTIONAL resampling envelope that only reweights observed same-scene
timing repetitions. This envelope is NOT a population confidence
interval or a significance proof about new scenes.

## No-new-benchmarks post-run command

    cd ~/Desktop/Programming/kyoto-common-contact
    git pull --ff-only origin research/kyoto-decision-coverage
    python3 scripts/analyze_kyoto_phase18_aa_robustness.py --aa output/kyoto-phase18-aa-mac/AA_RECHECK.json --output output/kyoto-phase18-aa-original-robustness --draws 2000
    open output/kyoto-phase18-aa-original-robustness/AA_ROBUSTNESS.md

## Optional corrected independent-label order study

First rebuild ccp_material_replay from this updated branch. To verify
the previously promising cloth/rapid N=1058 regime against a contrasted
sponge/rapid N=1331 regime, without exporting scenes again:

    cmake --preset kyoto-final-cpu
    cmake --build build/kyoto-final-cpu --parallel 4 --target ccp_material_replay
    python3 scripts/recheck_kyoto_phase18_aa.py --results output/kyoto-decision-mac-first --replayer build/kyoto-final-cpu/ccp_material_replay --output output/kyoto-phase18-aa-balanced-cloth-sponge --repeats 4 --materials cloth sponge --regimes rapid --min-n 1000

This optional *selected-condition* follow-up is not a complete-suite
validation and must not replace the original result or be pooled with it.

## Research decision

Keep frozen V4 intact as a promising guarded-reuse-to-SAP controller
in the specific sustained expensive-rebuild regime. The broad
performance improvement outside the positive rapid-cloth fixture
is near zero and not proven robust. Next research should vary real
geometry/topology and validate end-to-end solver and CCD accuracy
rather than perpetually benchmark only fixed AABB phase offsets.
