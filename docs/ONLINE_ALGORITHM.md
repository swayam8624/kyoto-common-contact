# Online Contact-Work-Aware Partitioning — implemented research experiment

**Status:** Actual C++23 implementation; collision-coverage and full physical
state equivalence tested. This document does **not** assert a new historically
unpublished broad-phase architecture. The implemented engineering
contribution is a safe, observable *adaptive policy*, subject to comparison
with PhysX ABP/PABP, dynamic BVHs and recent deformable simulation research.

## Problem

Three continuously forced deformable cloth sheets generate spatial
collision-candidate queries five times per timestep. A single global guard
cache reduces rebuilds but can inflate its candidate list so much that
precise vertex–triangle and edge–edge processing becomes significantly more
expensive. Uniform fixed-size cloth patches avoid many false candidates, but
a universally optimal partition size does not exist: sparse and dense
contact regions need different candidate-maintenance effort.

**Hypothesis:** local decisions from actual AABB work and confirmed contact
activity can adjust the region decomposition *at runtime*, while each query
returns the same required current-AABB candidate pairs as exhaustive search.

## What is online

\`OnlinePartitioner\` is updated after **every full simulation timestep**. A
new \`Policy::OnlineContactAware\` calls the existing \`RegionalCandidates\`
service within the existing five physics solver iterations. Each query
collects local *observed* counts:

- per-region exact bounding-box overlap tests during cache block rebuilds;
- checked cached pair IDs, number retained, and guard-related rejects;
- certificate invalidations and motion-bound changes;
- **verified positional contact corrections** actually executed by the
  unmodified sphere–triangle or inter-sheet feature solver;
- total broad-phase elapsed time, number of queries, and current
  conservative region-union overlap density.

The online controller uses no future collision ground truth and no manual
"this frame is busy" label. Its inputs are available at runtime.

## Local adaptive partition

Each simulated cloth has a fixed 12×12 **mesh cell** domain, with 2 triangles
per cell. The controller initially creates four 6×6 topological patches per
cloth. A patch can be split into four 3×3 children; four quiet 3×3 children
can merge into one 6×6 parent; four quiet 6×6 children can merge into a
12×12 parent.

The topology always partitions **mesh cell identities**, not the shape's
moving position in world coordinates. This is important: folding and
stretching do not change a primitive's membership unexpectedly. A patch ID
is a stable encoding of \`(sheet, level, meshX, meshY)\`.

For each patch \`r\`, at full frame \`t\`, collect a rough work proxy:

\`\`\`
W_r(t) =
    rebuild_AABB_tests
  + 0.65 * cached_candidate_rechecks
  + 2.00 * rejected_cached_candidates
  + 3.00 * invalidations
\`\`\`

The contact signal \`K_r(t)\` counts confirmed positional corrections, not
broad-phase candidates. The controller also tracks received candidate
counts \`N_r(t)\`. Every signal is exponentially smoothed:

\`\`\`
EWMA_r(t) = 0.78 * EWMA_r(t-1) + 0.22 * observation_r(t)
\`\`\`

At six-frame windows, a region is considered *hot* if observed work exceeds
a span-dependent threshold and contact/candidate activity is sufficiently
high. Two consecutive hot windows allow a split. Four siblings must each
remain quiet for three windows before a merge. At most one structural edit
is performed per decision, with a 12-frame cooldown.

These numerical thresholds are current **heuristics**, not optimized
parameters or a proven minimax/optimal cost model. The empirical
measurements determine decisions; the predictive cost function is not yet
learned from an independent training dataset.

## Runtime search strategy

The controller also observes the fraction of overlapping region-union
envelopes. Every six frames it can select between:

1. a flat enumeration of region pairs, or
2. a sorted-axis sweep of region-union envelopes, followed by certified
   per-region-pair cached candidate processing.

It uses a conservatively bounded overlap-density rule and smoothed actual
broad-phase time for both modes when both have enough samples. Strategy
changes have an 18-frame cooldown. **A strategy-switch count is not a
same-frame A/B performance proof**: the geometries and cache warmness
differ across measurement windows.

## Correctness and dynamic invalidation

Let the current padded primitive AABB be \`B_i(t)\`, the cached envelope
for its current patch be \`E_i\`, and the cached pair block for two patches
be \`C_pq\`.

For a valid certificate, every current \`B_i(t)\` is contained in its
corresponding cached \`E_i\`. Therefore every current primitive-pair AABB
overlap was also an envelope-pair overlap at cache creation.

The algorithm:

1. builds the CURRENT primitive bounds and eligibility masks;
2. certifies each region's cached envelopes;
3. if a patch region changes or escapes its bounds, recomputes dependent
   candidate blocks **before** returning candidates;
4. uses top-level envelope sweep to prune impossible region pairs;
5. **rechecks each cached primitive pair against CURRENT tight padded AABBs**,
   removing stale false-positive candidates but retaining required overlaps;
6. sorts and deduplicates primitive ID pairs, returning only candidate IDs;
7. delegates all precise geometry and physical responses to the existing
   solver, unchanged.

After any online split, merge, or traversal strategy switch, the regional
candidate service's cached certificates are invalidated. This is currently
a full certificate reset on structural changes (conservative but expensive),
rather than an incremental certificate remapping. Its cost is included in
subsequent real measured steps.

One subtle caveat: these are *discrete current-AABB* guarantees, **not**
a continuous time-of-impact proof, cloth self-collision handling, or an IPC
barrier nonpenetration certificate.

## Actual verification

\`tests/research_multicloth_tests.cpp\` includes:

- synthetic changing-contact feedback with actual split **and** merge;
- patch coverage (every mesh cell exactly once, no holes/overlaps);
- online search strategy switching under changing region union occupancy;
- three physically coupled cloth sheets running alongside SAP for 96 frames;
- exact coordinate equality within 1e-8 across all particles, despite
  different broad-phase policies and online repartitioning.

\`apps/online_controller_trace.cpp\` independently runs longer paired
real-physics sequences with periodic exhaustive AABB candidate checks.
Each frame records actual split/merge events, active patches, confirmed
contact corrections, candidate counts, measured broad/whole CPU step
times, controller decision time and SAP reference error. The CSV and chart
are regenerated on every machine; no numbers are hardcoded into diagrams.

A verified Linux CI trace of 120 frames reported **10 actual splits,
0 actual merges, 4 actual front-end strategy switches**, 9 independently
audited frames, and zero particle-state divergence. Real physical contact
was continuously rich, which explains why the controller did not merge
during that window. Controlled quiet synthetic feedback DOES test merging
but is not passed off as a real cloth-physics merge.

This trace recorded online **11.189 ms/step** versus SAP **11.401 ms/step**.
Other benchmark runs or hardware may reverse the ordering. Timings are
single-run CPU measurements, not confidence intervals.

## Reproduction

\`\`\`sh
brew install bullet
git pull --ff-only origin main
./scripts/prepare_meeting.sh
./open_multicloth_demo.sh
\`\`\`

The native window starts in online mode; \`Q\` cycles existing policies and
\`T\` toggles online vs fixed, while \`G\` opens the live events. Controls
\`P\`, \`J\`, \`Z\`, \`Y\` switch to fixed or offline-profile experimental
settings for ablations. Region splitting, contact heat, active patch count
and controller work are reported live. Inspect:

- \`output/native/online-controller-trace.csv\`: source data;
- \`output/native/online-controller-trace.svg\`: two-panel actual measured
  temporal chart with split/merge markers;
- \`docs/ONLINE_CONTROLLER_RESULTS.md\`: generated verification summary;
- \`output/native/multicloth-real.svg\`: seven-way full solver comparison;
- \`output/native/broadphase-leaderboard.svg\`: distinct **isolated-query**
  nine-way comparison including actual Bullet DBVT.

## What remains scientifically unproven

Online split/merge, BVH hierarchy, fat bounding volumes, SAP, runtime
adaptation and contact-aware culling have prior art. PhysX supports
automatic/parallel box pruning; dynamic deformable BVH maintenance and
curvature-based cloth collision culling have published work. This prototype
**does not establish that this combination has never been proposed**, that
its thresholds minimize expected total cost, or that it beats the best
published broad phase on realistic large scenes. Stronger next experiments
need independent complex cloth datasets, reproducible repeated CPU/GPU
trials, IPC/OGC/CCFR adapters, PhysX ABP/PABP comparisons, scalability and
a genuine incremental certificate re-mapping after local splits.

Useful references:
- https://github.com/NVIDIA-Omniverse/PhysX/blob/main/physx/include/PxBroadPhase.h
- https://github.com/bulletphysics/bullet3
- https://doi.org/10.1109/TVCG.2026.3729180
- https://doi.org/10.3390/math14091504
