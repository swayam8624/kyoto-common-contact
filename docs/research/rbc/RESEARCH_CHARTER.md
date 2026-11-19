# Reach–Budget Calculus: Research Charter v0.1

**Status: NEW research branch, NOT a validated paper, not a production acceleration.**
Branch: research/reach-budget-calculus.
Protected predecessor: research/kyoto-decision-coverage at 0809dea20d6b186f68fcb1acac4ab7200984e24f.
Archived recovery: archive/pre-cleanup-2026-11-19.
Snapshot initiated: 19 November 2026.

## 1. What problem are we REALLY trying to solve?

A collision solver does not fundamentally ask whether two UNINFLATED
axis-aligned boxes overlap at a single instant. It asks what pairs could
become relevant under a contact-distance threshold, solver movement,
and the interval over which contacts/narrow-phase results are consumed.

Existing Kyoto V4 answers exact CURRENT AABB intersections using
Guard versus persistent SAP. Its useful result is selective *CPU query*
speed: on a 54-scene 60-frame rapid-motion cohort, 1.0577x Guard/V4
geomean on Apple Silicon (39/54 wins, 11 actual methods) and 1.0552x
on Linux (25/54 wins, 10 methods). 18/18 condition-level aggregate
SAP/trial/switch counts matched across both platforms. This is
not a measured solver-integrated adaptive speedup: on the independent
real 60-frame coupled contact solver, frozen V4 stayed Guard and natural
same-cloth self-contacts were zero.

The *new question* is sharper and much riskier:

Can a shared **solver-contracted interaction service** safely reuse
candidate relationships across actual solver iterations, long coherent
deformation and mixed discrete/swept queries, paying only for invalidated
reach certificates and necessary pair consumption, while beating the
strongest honest fixed broad phase on real contact-rich solvers?

Here, "shared" means compatible semantic contracts and audited adapters,
NOT a universal physics engine or a vague common API. The system must
expose *which* nearby pairs the consuming solver needs, *when* previously
computed pairs remain provably sufficient, and how to pay for rebuilding.

## 2. The hard scientific boundaries

**Before implementing optimization, distinguish these objects:**

- AABB candidate: a possible contact, NOT a contact.
- Current reach-inflated AABB candidate: boxes may be within coordinatewise
  l_infinity separation r_i+r_j. This does NOT assert Euclidean primitive
  surface distance, triangle contact, mesh intersection or CCD.
- Swept candidate: conservative envelope over a specified full motion path.
  Endpoint hulls alone are sufficient for *linear vertex motion* only.
- Narrow-phase candidate: solver-specific vertex-face/edge-edge/etc.
  filtering with adjacency/mask/category semantics.
- Contact response: solver-specific constraint/barrier/impulse application.
  Exact broad phases feeding identical solver states SHOULD produce
  the same actual geometry, not improved-looking folds.
- Penetration-free dynamics: a solver guarantee conditional on all its
  own assumptions; an RBC candidate superset does not establish it alone.

**Publication gate:** never call an algorithm novel merely because it
uses "adaptive", "certified", "budget", "coherent", "ML", "GPU",
"topology-aware" or "common". All these concepts have prior art.

## 3. Concrete query contract v0 (already implemented as correctness oracle)

Given a stable ordered list of finite primitive AABBs B_i(t),
static nonnegative per-primitive contact reach r_i and skin m_i,
define the nonnegative l_infinity box gap:

  gamma(A,B) =
    max(0, min_B.x-max_A.x, min_A.x-max_B.x,
           min_B.y-max_A.y, min_A.y-max_B.y,
           min_B.z-max_A.z, min_A.z-max_B.z).

Current exact reach-AABB oracle:

  O(t) = {(id_i,id_j): id_i<id_j, compatible_t(i,j),
                          gamma(B_i(t),B_j(t)) <= r_i+r_j}.

At reference time t0, store potential pair indices S by:

  (i,j) in S if gamma(B_i(t0),B_j(t0)) <= r_i+r_j+m_i+m_j.

If ALL six AABB faces of EACH primitive have moved by at most m_i
since t0, then exact current filtering of S equals O(t).
The gap is 1-Lipschitz in the sum of the six-face infinity motions.
The code detects skin violation from actual current faces and
rebuilds the ENTIRE O(N^2) potential set exactly.

**Important implementation design:** The stored potential set includes
pairs whose category/mask currently exclude them. Masks can change later
without creating missed pairs; compatibility is applied only at current
filter. Stable ID order is mandatory. Invalid input throws rather than
quietly accepting incorrect candidates. This Stage-0 implementation
uses O(N^2) construction and O(|S|) filtering. It is NOT a fast broad
phase, no cost advantage is claimed and no production selector is touched.

Source: include/ccp/rbc/reach_budget.hpp.
Oracle/regression: tests/rbc_reach_budget_tests.cpp.
CTest: rbc_exact_reach_certificate_and_invalidation_oracle.
Important independent control: ccp::rbc::exhaustive.

## 4. RBC mathematics: PROVEN versus CONDITIONAL versus NOT YET PROVEN

### A. Geometric gap bound — mathematical lemma

For valid axis-aligned boxes, define D_i(t,t0) as the maximum absolute
motion of any of the six AABB face coordinates. Then

  |gamma_ij(t)-gamma_ij(t0)| <= D_i+D_j.

Reason: each separating-axis gap expression changes by at most
D_i+D_j; max of finitely many terms is nonexpansive in max norm.

Therefore stored reference potential pairs are sufficient under
D_i<=m_i and D_j<=m_j. All originally excluded pairs stay excluded.

**Important numerical caveat:** A real implementation must also
validate floating-point boundary cases, NaN/Inf, overflow,
rounding and skin epsilon. "Exact" in our native tests means
the same real-valued AABB contract evaluated with robust machine
arithmetic, not an arbitrary-precision theorem on IEEE 754.

### B. Tightness — only in the worst case

In one dimension, two boxes can move directly toward one another.
If their total allowed displacement exceeds the original free gap,
they can touch. Therefore NO certificate based only on scalar
initial gap and worst-case unsigned motion can safely assume that
excluded pair stays excluded in ALL possible trajectories. This is
an existential worst-case construction, not a statement that every
real pair will touch.

### C. Ledger-based certificate — sufficient if solver bounds are honest

A solver can supply known upper bounds delta_i(s) before each step.
The cumulative ledger L_i=sum delta_i(s) upper-bounds face drift
*only if* each supplied bound truly covers that motion.
A budget is valid while L_i<=m_i. The Stage-0 engine ALSO checks
actual reference face displacement, so it falls back safely even
if a solver overestimates its own honest budget or forgets hints.
Unknown or unbounded delta => rebuild or certified expanded hull.
A "one time-step" δ is NOT enough for unrestricted Newton iterates.

### D. Swept / CCD — conditional on actual path enclosure

For *linear vertex paths*, the primitive's AABB at ANY sub-time
is contained in the union of its endpoint AABBs. Thus the componentwise
endpoint AABB hull is a conservative swept envelope; an exact
swept-envelope query can identify broad candidates.
**For curved, accelerated or unknown motion, the endpoint hull
alone can miss a mid-step collision.** Require independently proved
path-envelope bounds. Even a correct swept broad phase does NOT
solve continuous narrow-phase triangle collision detection.

### E. Translation-compensated certificates — global Stage 0 IMPLEMENTED

Two primitives undergoing exactly the SAME common translation preserve
their box gaps. The Stage-0 oracle now has query_translated(), accepting
a solver-supplied *cumulative* whole-scene translation. It subtracts
that translation from every CURRENT six-face displacement before checking
the reference skin. A shared translation of 1000 world units may
therefore preserve the original potential pair certificate rather than
force a stale-cache rebuild, provided relative face residual is
at most skin_i. False/missing hints are safe: direct measured residual
validation forces an exact rebuild. This is accompanied by
tests/rbc_reach_budget_tests.cpp property and negative-control cases.

**NOT YET IMPLEMENTED:** multiple independently translating clusters,
hierarchical patch fronts, inter-cluster relative-motion certificates
and solver-predicted prospective motion bound consumption. For
separate clusters, a single global translation cannot be cancelled
for each pair; validity needs a checked inter-cluster relative
translation plus two residual bounds. A mesh partition must not
silently ignore nonlocal folded contacts or interactions between
different sheets.

A *measured* residual after motion is valid for an a posteriori
candidate check. A *predicted* residual used to skip query work
before motion must be a GUARANTEED bound, not an EWMA or a visual
estimate. No performance advantage is claimed from the current
O(N^2) reference-list implementation.

### F. Margin cost model — an idealized, falsifiable model

For homogeneous box scale a, density factor kappa and mean displacement
delta, one proposed proxy for expected cost per primitive is

  C(m) = c_build*delta/m + c_filter*kappa*(a+2m)^3.

For m>0 and positive parameters the proxy is strictly convex;
its stationary point solves

  m*(a+2m) = sqrt(c_build*delta/(6*c_filter*kappa)).

This gives a testable small-margin sqrt(delta) scaling and a
different large-margin delta^(1/4) regime. It is NOT a general
physics theorem: heterogeneous geometry, correlated motion,
distribution tails, fat bounds, cache locality and non-3D
density break the proxy. The optimal margin cannot be asserted
without measuring full paid work, validating model residuals
and comparing against tuned fixed-margin Guard.

### G. Graph slack allocation — optimization goal, not deployable yet

For excluded pairs with nonnegative slack g_ij, a sufficient condition
for a new pair NOT to appear is D_i+D_j < g_ij (strict inequality
because inclusive touching counts as contact). A pairwise
budget split weighted by sqrt(c_i*delta_i) minimizes a
specific c_i*delta_i/b_i proxy subject to b_i+b_j<=g_ij.
For a whole graph, obtaining *the nearest excluded pair*
might itself cost O(N^2). The graph solver cannot assert
O(|D|log N) maintenance until it has an actual indexed
dynamic representation and measured amortized proof.
Handle two immobile endpoints, zero δ and vanishing gaps
explicitly; division by zero is not a mathematical algorithm.

### H. Online rent-or-rebuild guarantee — NOT ACCEPTED AS A THEOREM YET

The provided ski-rental bound requires a precise adversarial
cost model, feasible offline comparator, a chosen timing of
rent/rebuild, and proof that rent is monotone with staleness.
Those assumptions are not established for real Guard caches
and dynamically changing collision candidate sets. Treat any
"2+rho_max/c_build competitive guarantee" as a proposal
subject to counterexample tests and independent proof review.
Do not publish the current sketch as a theorem.

## 5. Prior art that seriously threatens novelty

The following were looked up on or before 19 November 2026;
these are *specific comparison targets*, not a complete survey.

| Work | Direct implication | Primary source |
|---|---|---|
| LAMMPS Verlet neighbor lists and skin | Reuse safe while displacement budget remains is decades-old; cannot claim generic skin novelty | https://docs.lammps.org/Developer_par_neigh.html |
| IPC Toolkit (LBVH/spatial hash/CCD) | Common reusable collision interface and inflation-radius broad phases exist; needs real integration and matched semantic comparison | https://github.com/ipc-sim/ipc-toolkit |
| IPC Toolkit latest LBVH releases | A strong new default external comparator; older DBVT-only comparisons are insufficient | https://github.com/ipc-sim/ipc-toolkit/releases |
| Offset Geometric Contact, SIGGRAPH 2025 | Vertex-specific displacement bounds already underwrite guaranteed collision handling; solver-bound semantics alone are NOT new | https://graphics.cs.utah.edu/research/projects/ogc/Offset_Geometric_Contact-SIGGRAPH2025.pdf |
| Dynamic Adaptation of Broad Phase Collision Detection Algorithms (2011) | Backend switching is established prior art | https://hgpu.org/?p=4512 |
| Parallel context-aware sweep and prune | Adaptive spatial/broad-phase algorithms are established | https://pubmed.ncbi.nlm.nih.gov/28574360/ |

**Next prior-art action:** read IPC Toolkit's actual current broad_phase
source and OGC reference implementation at pinned Git SHAs. Check
incremental candidate invalidation, Newton/line-search reuse,
swept bounds, per-primitive radii, reuse across solver iterations,
deformable topology, and full cost accounting line by line.
Only THEN write a claim-by-claim gap matrix. Do not infer absence
of existing functionality from README feature lists.

Our *possible* differentiator is not any one primitive lemma;
it is the **verified, low-overhead composition** of:
solver-supplied reach, translation-compensated local certificates,
variable per-primitive motion budgets, an analytically calibrated
crossover policy and a single conservative contact-frontier consumer,
evaluated end-to-end under independent solver workloads.
Even that is only a hypothesis until prior art and experiments
establish a meaningful new capability.

## 6. Engineering modules and separation

Build v0:

    Solver-specific adapter
          |
          v
    Reach + motion contract (r_i, max delta_i, masks, identity)
          |
          v
    O(N^2) reference candidate certificate + exhaustive CURRENT oracle
          |
          v
    Exact sorted/canonical pair IDs
          |
          v
    Solver-specific narrow phase/CCD/contact response

Next modules (ALL gated by correctness oracle):

A1. Reference certificate updated by dirty regions; never omit pairs
    when a region is invalidated, temporarily rebuild or fall back.
A2. Adaptive-margin Guard CONTROL: treat tuned fixed margin
    as a serious opponent; fit m-v curve on training-only data.
A3. Spatial hash and refit BVH with full pair output and fair tuning.
A4. Cluster-relative certificates for coherent non-local movement
    with topology-aware invalidation and static/dynamic grouping.
A5. Cost-aware fixed-backend scheduler only if observable cost
    headroom remains after A2 and A3; never train on test cases.
A6. Geometry and contact front as a canonical pair-owning DAG
    only if measurable region-local benefit emerges at large N.
A7. IPC Toolkit adapter for one precise contact/CCD contract;
    separate PolyFEM/XPBD/OGC adapters are future work.
A8. Output-sensitive CPU/GPU pipeline only after end-to-end
    profiling warrants it; count-only GPU kernels cannot claim
    an accelerated full pair-stream consuming physics solver.

No existing V4 thresholds, historical worktrees, negative evidence,
train/test splits or old benchmark scores are modified by this branch.

## 7. Benchmark ladder — NEVER merge across levels

L0: Mathematical correctness oracle, exhaustive reach AABBs.
    Verify touching, masks, ID changes, degenerate AABBs,
    motion-bound violations, static masks, dynamic masks,
    adversarial and randomized displacements, restart/reset.
    This is the ONLY level currently implemented for RBC.

L1: Immutable material AABB replay with source receipts and
    exact full per-frame pair set equality to oracle.
    New independent forcing schedules, not just same schedule
    with varying node seeds. Distinguish baseline and rapid.

L2: Fully materialized pairs, byte counts, canonical sorting,
    duplicate prevention, memory/RSS, streams to a real consumer.
    Count-only/digest-only is a separate metric.

L3: Coupled physical solver. Compare particle trajectories,
    narrow-phase executed counts, strict independent pair
    oracle on smaller meshes, actual nonlocal self contacts,
    real contact response and solver stepping costs.

L4: Real contact-rich external solver workloads. Report
    whole-step runtime INCLUDING all updates and method
    selection, plus CCD/contact quality and penetration
    audits independent from the implementation.

L5: Fresh scenes and hardware, untouched final cohort,
    independent implementation or investigator replication,
    intervals across *independent scenes*, not 60 correlated
    frames or repetitions of one motion schedule.

### Baselines all given honest tuning effort on development data

- brute-force oracle only for L0, not competitive large-N timing;
- frozen Global Guard at historical fixed 0.045 margin;
- tuned fixed-margin Guard AND velocity-aware margin Guard;
- frozen V4 with actual trial/switch bookkeeping;
- rebuilt and persistent SAP; newer kinetic incremental SAP where available;
- tuned spatial hash / hashed grid;
- refit/rebuild BVH, actual IPC Toolkit LBVH and HashGrid;
- optional genuine Bullet persistent DBVT with proxy lifecycle verified;
- narrow-phase/solver-native original collision path;
- output-consistent threaded CPU and genuine GPU baselines.

**Baseline fairness:** equal numerical tolerance, candidate semantics,
hardware, exact output, host/device transfer accounting, same reference
geometry, repeat schedule, memory and solver settings. Different
contracts do not belong on one deceptive leader board.

### Conditions chosen to falsify specific mechanisms

- coherent fast translation (RBC should win via translation compensation
  ONLY if implemented);
- incoherent fast rotation or oscillation (expected failure regions);
- nearly static with high pair density (bookkeeping may dominate);
- rapid fold with real nonlocal same-sheet self contacts;
- very large stationary background and tiny moving contact hotspot;
- pair output all-overlap K=Theta(N^2) (no forbidden sublinear claim);
- jumping/topology change (mandatory invalidation);
- dense stacked soft bodies, tearing or growth with new stable IDs;
- solver steps with variable Newton/line-search displacements;
- high N: 1k, 4k, 16k, 64k, 256k, 1M (distinguish primitives from triangles).

No fixed "frame 32" transition reused as the only motion event.
Set new forcing schedules and seeds before the final test, archive
the split manifest hash, and never revisit it after tuning.

### Timing, statistics, negative evidence

ABBA/repeated independently initialized method copies and
randomized order, CPU affinity when supported, controlled
thermal conditions, same compiler flags, recording a timed
A/A noise floor in EVERY session, ≥30 repetitions per
source scene when needed to measure small effects.
Aggregate log-ratios per independent scene; clustered bootstrap
over scenes and source families. Report medians/IQR,
mean and P95/P99; fixed win/loss condition matrices,
time/memory/accuracy Pareto, construction versus query,
build/repair/certification/selection/copy/CPU-GPU sync.
Never bootstrap frames as independent observations.

The 2026 V4 experiment already revealed how deceptive
a 5% geomean could be (Mac 39/54 wins, Linux 25/54 wins).
No RBC "publication-level win" is pre-established.

**Aspirational success gates, NOT achieved:**
G1. Strict Oracle pair parity on every supported reach contract.
G2. At least one meaningful high-contact L4 physics solver
    with nonlocal contact and independently validated correctness.
G3. At least 1.10x end-to-end speedup on a pre-registered
    independent contact-rich cohort where broad phase has
    a meaningful wall-time share, interval excluding 1x.
G4. Win or principled crossover over best tuned deployable
    baselines; no catastrophic tail regression hidden.
G5. Novelty matrix showing what prior work cannot already do.

Failing any gate means revise the claim, not rename a phase.

## 8. Paper-level visual correctness (not cosmetic)

When two exact broad phases feed the SAME solver, their geometry
SHOULD be identical. No invented more-beautiful cloth folds.
A fair "before/after" is:

(a) SAME real mesh and camera, overlay actual per-frame
    costs (build, query, filter, narrow phase, solver).
(b) SAME compute budget, show quantitatively higher mesh
    resolution/iterations/contact accuracy if the savings
    are actually consumed that way.
(c) GUARANTEE test: a deliberately unsafe, labeled
    negative-control cache drops a required pair; a true
    per-frame penetration oracle then identifies the
    resulting error. Never confuse this with a baseline.
(d) SAME full pair IDs, show where geometry allows
    certificates to remain valid versus rebuild.

Required panels:

1. One contact-rich physically rendered hero with a labeled
   SOURCE mesh, not a smoothed invented "truth" mesh.
2. All-algorithms time-locked exact mesh grid; AABB bounds,
   potential pairs, real narrow-phase contacts and
   state parity counters.
3. Budget visualization: per-primitive remaining certified
   skin, relative residuals and refreshed patches.
4. Prediction-vs-measured crossover heatmap with measured
   scene dots and actual negative points.
5. Independent per-scene before/after log scatter y=x
   (every loss remains visible).
6. Full wall-time decomposition with actual per-query
   overhead, P50/P95/P99 spikes and cache pressure.
7. A/A noise controls alongside the measured improvements.
8. Penetration, minimum gap, CCD/toi, strain and energy
   diagnostics versus real known reference.
9. Scaling/log-log with N and exact emitted K; memory/RSS.
10. Failure-case gallery: high-density, oscillation,
    teleport, nonlinear sweep and topology changes.

Reproducible figure pipeline only:
immutable source snapshots + SHA -> verified derived CSV/JSON
-> matplotlib/Blender/ParaView scripted views -> vector PDFs,
PNG previews and synchronized MP4/GIF. Every figure has
source hash, method, scene, seed, machine, metric, sample unit.
No stock renders passed off as actual simulation data.

## 9. Immediate task queue

1. Leave the prior archive branch and original SHA history intact.
2. Complete the Mac/Linux 3,240-frame source trace comparison when
   the Mac .json.gz bytes become accessible; its local
   SHA-256 was 19dabf9b3b1ffd17128d6f699fa2e9aafa18cba273c29da3777d64e25fdd2754.
3. Pass new rbc_exact_reach_certificate_and_invalidation_oracle on
   Linux C++23, ASan+UBSan and full existing CTest regression.
4. Add independent property generation and a higher precision
   coordinate oracle on exact boundary tests.
5. Fit cost model from new untuned development data; measure
   whether adaptive-margin Guard eliminates V4's advantage.
6. Inspect IPC Toolkit, OGC and published collision detection
   code before proposing new sufficient-condition theorems.
7. Pick ONE solver adapter (IPC Toolkit preferred only after
   actual C++ API review), profile broad-phase fraction of
   complete simulation and independently test real contact.
8. Build topology-local cluster-relative frontier only if
   measurable headroom remains, use fallback when certificates
   fail or motion bounds are unknown.
9. Pre-register untouched physical test scenes and build
   reproducible paper graphics from *actual* solver output.
10. Promote nothing until correctness, costs and novelty have
    survived the most adversarial independent evaluations.

## 10. Permission boundaries for future agents

NEVER delete negative controls or older source merely because
a newer algorithm is more attractive.
Never rewrite Git history, change frozen V4, relabel a
synthetic mass-spring surrogate as calibrated soft-body physics,
claim measured solver gain from current AABB replay, or
tune on an already-inspected test set.
A failed test is a result; keep the failure and repair the
algorithm, not the threshold of what counts as correct.
