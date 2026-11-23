# Pass 13 — Real FEM broadphase baseline, without a fake SCA speedup

## Scope

This pass deliberately establishes an *honest lower-level experimental
baseline*. The source generator is the genuine C++23 180-node,
480-tetrahedron neo-Hookean finite-element body against the original
72-face nonplanar rigid triangular obstacle. It advances **360 physical
macro-steps at 0.00075 s** and samples the actual vertex states every
12th integrated step. This includes precontact and contact-bearing source
states because the original Pass 11 first contact occurs near 0.21975 s.

The 360 integrated steps are real physics, not a scripted mesh animation.
The 30 sampled motion intervals use physically generated vertex endpoints.
Straight Bezier paths between the sampled pair of consecutive *macro* states
are valid for those macro-step endpoints; **they are not a reconstruction
of any internal adaptive subdivision, acceleration curvature, or the
true solver's continuous contact-time path**. For exact continuous CCD on
full solver trials, capture accepted physical leaves and compare equivalent
motion models.

## What is measured

The new executable `kyoto_pass13_reference_fem_benchmark` constructs
a full connected feature mesh containing original deformable exterior
triangles and original static rigid patch triangles. Topology is explicit,
stable and checked. Its shared vertices, edges and faces contain both
cross-object and topologically admissible self-object candidate possibilities.

Every sampled original FEM source geometry is transformed into the
same conservative linear-trajectory cubic-control hull for both methods.
The methods are:

1. **Exhaustive reference**: independent vertex-face and edge-edge
   enumeration with explicit adjacency filtering.
2. **Rebuilt SAP**: sweep-and-prune using the exact same feature bounds
   and filtering, with no persistent maintained frontier.

Both include their *own* full mesh/box construction time and candidate
enumeration/normalization time. A candidate-set mismatch terminates
the native executable. The experiment records per-frame raw nanoseconds,
feature counts, source physical time and genuine impulse-update counts.

Physics time and visual rendering are excluded from these timings.
The benchmark currently runs one fixed-order reference/SAP sample per
physical snapshot. Cache effects, CPU scaling, memory-allocation variance,
timing noise, process order bias, second integration and all official IPC
comparisons remain uncontrolled. This is a *functional/cost-instrumentation
gate*, not publication-grade performance evidence.

## Reproduction

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kyoto_pass13_reference_fem_benchmark --parallel 4
./build/kyoto_pass13_reference_fem_benchmark results/pass13/native
python3 -m pip install matplotlib
python3 scripts/report_pass13_fem_baselines.py \
  --input results/pass13/native/native_fem_reference_broadphase.csv \
  --output results/pass13/science
```

The new GitHub Actions workflow
`.github/workflows/pass13-fem-reference.yml` independently attempts this
on macOS and Ubuntu, archiving raw original CSV and reporting charts.
**Do not quote performance results until the workflow actually succeeds.**

Outputs:
- `native_fem_reference_broadphase.csv` — raw C++23 source records;
- `pass13_reference_summary.json` — paid ratio-of-totals and paired
  per-snapshot ratios, as explicitly provisional;
- `13a_baseline_costs.png` — paid native reference and SAP build+query;
- `13b_paired_baseline_ratios.png` — sampled paired ratios with contact
  state from true FEM source receipts, not invented labels.

## Next non-negotiable extensions

**Gate A: source trajectory rigor.** Record every accepted substep and
its actual geometry, elapsed duration and topology epoch. Preserve exact
source hashes and compare the same motion model and feature filtering
to official IPC.

**Gate B: actual pinned IPC Toolkit v1.6.1.** Reuse the existing
`tests/ipc_lbvh_parity_tests.cpp` adapter, pin exact release SHA,
add actual timestamped build/query and candidate counts on IDENTICAL
source snapshots and declare precision/filter differences. Correctness
must be a coverage condition, not necessarily exact pair equality,
because IPC float LBVH may be deliberately conservative.

**Gate C: genuine maintained SCA.** Implement a persistent dual-tree
frontier with checked topology epochs, local refit, full-rebuild fallback,
cache and invalidation charges. A rebuilt sweep is NOT an SCA method.

**Gate D: statistical comparability.** Counterbalanced randomized
runs, A/A trials, held-out scene families, P50/P95/worst cost, per-trajectory
geometric means, paired uncertainty estimates and hardware/compiler logs.

**Gate E: contact-rich validation.** Multiple independent real physical
solvers, moving rigid meshes, closed shells, deformable self-contact and
independent CCD event results. The current reference is one small
static-obstacle FEM integration.

The presence of scientific charts is not evidence of SCA novelty.
A future SCA branch must first be safe and then beat the properly
instrumented independent baseline. All negative results are retained.
