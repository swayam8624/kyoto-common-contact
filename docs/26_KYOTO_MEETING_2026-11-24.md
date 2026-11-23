# Kyoto research meeting — 24 November 2026

**Topic:** From physically real 3D contact to a maintained Spacetime Contact Atlas
**Project:** `kyoto-common-contact`, branch `research/spacetime-contact-atlas`
**Audience:** Kyoto research supervisors
**Scientific posture:** show executed evidence only; explicitly preserve negative results.
**Purpose:** request agreement on the next credible research hypothesis and validation protocol.

## Quickest reproducible Mac/Linux preflight

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/spacetime-contact-atlas
git pull --ff-only origin research/spacetime-contact-atlas
bash scripts/run_kyoto_meeting_preflight.sh core
# Only if upstream IPC Toolkit dependencies are available:
bash scripts/run_kyoto_meeting_preflight.sh ipc
```

The script stops on C++ build, oracle, source-physics or CSV validation
failure, records hardware/commit provenance, and saves all measured
receipts to `results/kyoto-meeting-2026-11-24/`. Do not present a failed
preflight as a completed experiment.

## Opening (about 45 seconds)

> After our earlier feedback about insufficiently varied and overly simple
> collision tests, I reset the project around physical 3D contact rather
> than only optimizing synthetic broadphase AABB counts. The work now has
> a true deformable 3D neo-Hookean finite-element solid contacting an
> actual connected, sloped rigid triangular surface. I separated
> physical response correctness, broadphase candidate correctness and
> speed measurement, so none is mistaken for the others. The next
> question is whether keeping an adaptive contact hierarchy across
> simulation steps can give a reproducible advantage over strong
> upstream IPC Toolkit LBVH, including all update costs.

## Evidence table — use these only with original receipts

| Claim | Native source measurement | Limit |
| --- | --- | --- |
| True 3D FEM physics | 180 volume vertices, 480 tetrahedra, 900 accepted macro-steps, 0.675 s integrated physical time | small single-body explicit solver |
| Nonplanar finite triangle obstacle | 49 indexed rigid vertices / 72 original rigid triangles | open static surface, not closed or moving mesh |
| Physically responding surface contact | 517/900 steps with impulses, 11,731 original face impulse updates | impulse update count is not 11,731 distinct collision episodes |
| Independent static finite-face audit | 901 original states have positive rigid/deformable finite triangle separation; min 1.736230314 mm | static accepted states and finite test cases, not a general floating-point CCD proof |
| FEM deformation quality | min det(F) 0.722870523, max von Mises stress 8,987.93 Pa | does not establish material calibration |
| Old sphere-only obstacle control | 95/181 sampled states intersect the *same new rigid mesh* | different implemented response law: negative illustration, not matched solver speed comparison |
| Real fast impact | 20 m/s approach; full .75 ms was integrated through 3 accepted leaves after 2 geometric rejections, positive 0.753194 mm accepted separation | finite scenario only; extreme 900 m/s regression remains unresolved |
| Original energy account | −389.744797 J mechanical energy change over 900-step finite-mesh source | physical losses not independently attributed to friction/damping/time discretization |

**Original independently verified Pass 11 CI:** 
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45160203229

**Pass 12:** explicit original-data energy audit and three scientific figures,
separating observed contact energy changes from causal interpretation.

**Pass 13:** actual 360 FEM macro-step integration with 30 sampled
endpoint-linear motion intervals; independent exhaustive, rebuilt SAP and
persistent-order SAP candidate sets checked against one another.

**Pass 14:** implemented real binary refittable node hierarchies with
two-tree VF and self-tree EE traversal, warm refits, cold invalidation
and 180 randomized independently-checked cubic motion test scenes.

**Pass 15:** official pinned IPC Toolkit LBVH compared with maintained
hierarchy on an identical shared real-FEM geometry adapter.
Official candidate coverage of the independent reference and hierarchy
exact parity are nonnegotiable pass/fail gates. Full paid conversion,
build, query, refit and normalization are recorded.

**DO NOT quote new Pass 13–15 speedup numbers until specific CI workflows
pass and original CSV/JSON artifacts have been inspected.**

## Suggested 4-minute executable demonstration

1. Show the **original verified Pass 11 visual atlas**: toroidal FEM body,
   sloped connected rigid patch and per-vertex von Mises stress. Explain
   that geometry and physical forces come from original C++23 simulation,
   not interpolated procedural animation.
2. Show **fast impact** from the original accepted physical leaves:
   the initial unsafe full step is rejected, smaller steps genuinely
   re-integrate and sum to the original requested duration.
3. Compile and run the **new maintained hierarchy's independent oracle
   checks**, then show the tree/hierarchy algorithm source and the
   candidate-set invariant.
4. Only if Pass 15 CI passes: show paired official IPC/native hierarchy
   CSV/plots, explain the precise scoped ratio, sample size, ordering,
   negative cases and missing generality. If CI is not green, show the
   implementation and test gate instead, *not* a fabricated speed plot.

Commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4 \
  --target kyoto_refit_feature_hierarchy_tests \
           kyoto_pass13_reference_fem_benchmark
ctest --test-dir build -R \
  'maintained_refit_hierarchy_VF_EE_oracle_and_invalidation' \
  --output-on-failure
./build/kyoto_pass13_reference_fem_benchmark results/kyoto-meeting-24nov/source
python3 scripts/report_pass13_fem_baselines.py \
  --input results/kyoto-meeting-24nov/source/native_fem_reference_broadphase.csv \
  --output results/kyoto-meeting-24nov/figures
```

Optional *actual external* IPC build:

```bash
cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release \
  -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
cmake --build build-ipc --parallel 4 \
  --target kyoto_pass15_official_ipc_fem_benchmark
./build-ipc/kyoto_pass15_official_ipc_fem_benchmark \
  results/kyoto-meeting-24nov/ipc
python3 scripts/report_pass15_official_ipc.py \
  --input results/kyoto-meeting-24nov/ipc/official_ipc_fem_paired.csv \
  --output results/kyoto-meeting-24nov/ipc/figures
```

## Our testable research hypothesis

> For deformable 3D geometry in which active contact remains concentrated
> and moves gradually, retaining a selectively refitted geometric
> contact frontier may reduce *total paid* candidate-generation and
> eventually entire collision-detection time against pinned LBVH,
> while never missing any reference-admissible VF/EE candidates.

Not yet proven: the actual selective frontier, independent solver-level
narrowphase event parity, multi-solver transfer, floating-point certified
general triangle CCD, or matched end-to-end speedup.

## The next honest experiment

- Freeze the source physical states/accepted *leaf* trajectories, topology
  epochs, masks, collision thickness and source hashes.
- Compare our hierarchy, rebuilt SAP, persistent SAP and **official**
  IPC Toolkit LBVH on precisely identical feature boxes.
- Candidate coverage against exhaustive oracle is a hard safety gate;
  record all extras and all misses, not just average counts.
- Instrument conversion, build, local refit, invalidation, hierarchy
  traversal, output normalization and memory allocations.
- Add severe controls: high motion, dense contact, total topology
  invalidation, cold start, extreme geometry and negative workloads.
- Counterbalance order and run A/A; multiple independent seeds/solvers,
  per-trajectory ratios, P95 worst cases, hardware/commit/flags, and
  clustered uncertainty. No novelty claim without a prior-art matrix.

## Questions for the professors (ask explicitly)

1. Is the most promising contribution a **certified selective spacetime
   frontier** on top of proven collision algorithms, or should we
   prioritize solver-specific integrated contact scheduling?
2. Which reference to establish as decisive for the next milestone:
   official IPC Toolkit LBVH feature candidates plus actual upstream CCD
   events, or an additional maintained BVH baseline such as DBVT?
3. Would a second independent physical solver and **moving closed shell /
   self-contact** be required before proposing a paper-level claim?

## If asked about shortcomings

- We have **not** shown a general speedup against official IPC.
- The new benchmark's 30 sampled snapshots represent macro-step
  endpoint-linear envelopes, *not all adaptive accepted contact leaves*.
- A positive Jacobian and nonpenetrating sampled surfaces are not a
  full converged physical calibration.
- Contact impulse updates are neither distinct events nor solver-wide
  performance metrics.
- Prior synthetic Guard/RBC wins and losses remain archived research
  background; do not mix those figures into the new SCA comparison.

## Closing (20 seconds)

> I am not asking you to accept that this is already faster or novel.
> I am asking whether this correctness-first experimental foundation
> and its negative controls are the right basis for a genuinely useful
> common contact broadphase, and which exact next research gate you
> would want to see before judging a publishable result.
