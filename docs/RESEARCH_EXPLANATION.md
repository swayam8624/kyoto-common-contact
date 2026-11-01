# Research primer — a conservative, request-aware collision-candidate service

**Kyoto University research discussion, November 2, 2026.** This guide distinguishes
the working prototype, the correctness argument, measured performance, and possible
future publication-level novelty. Do not conflate broad-phase benchmark data with
results from a completed IPC, OGC, or CCFR integration.

## One-sentence research question

Can a reusable broad-phase infrastructure safely **avoid redundant candidate
generation across changing geometry**, while preserving each caller's geometric
query contract (proximity, allowed pair types, geometry motion, CCD requirements),
and improve **total contact-handling time**, not merely isolated search time?

This question has several independent parts:

1. **Geometric correctness:** Does the output cover *all required* pairs?
2. **Temporal validity:** When can a prior candidate list be reused?
3. **Semantic interoperability:** Can different collision methods specify the
   correct pair eligibility, margins and motion bounds?
4. **Performance:** Do saved builds exceed certificate and extra precise-test
   costs in real workloads?
5. **Physical outcomes:** If an existing narrow phase and solver consume the
   candidate list, does the resulting state/convergence remain valid?

**Current evidence:** parts of 1, 2, and 4 for a single current-configuration
sphere–triangle cloth fixture; basic 3D synthetic multi-sheet broad-phase
coverage against an actual published-family Bullet DBVT implementation.
Parts 3 and 5 across external physics solvers remain prospective research.

## Where broad phase sits

At each configuration requiring collision detection:

  updated positions / predicted motion
      |
      v
  caller supplies primitive bounds + query semantics
      |
      v
  BROAD PHASE (cheap spatial candidate discovery)
      |
      v
  candidate pair list (conservative, possibly too large)
      |
      v
  NARROW PHASE (method-specific precise proximity / CCD / contact construction)
      |
      v
  contact constraints, feasible regions, manifolds, or barrier contributions
      |
      v
  METHOD-SPECIFIC SOLVER (XPBD, IPC, OGC, etc.)
      |
      v
  changed positions -> possible new broad-phase call

"Broad phase first" means first in *contact discovery*, **not** necessarily
the first operation in a simulation timestep. The current custom cloth solver
predicts positions, solves internal distance constraints, and invokes a new
broad-phase query inside each of eight solver iterations; after the iterations
it performs a final query for visualization. This is a design of the testbed,
not a universal rule for IPC or OGC ordering.

## Complete terminology, with precise meaning

- **Object:** an entity in the scene, e.g. a cloth sheet or sphere.
- **Primitive:** the smallest geometric item used for a collision query,
  e.g. sphere, triangle, segment, vertex, bounding proxy, or feature pair.
- **AABB:** axis-aligned bounding box: three independent closed intervals
  \`[min_x,max_x] × [min_y,max_y] × [min_z,max_z]\`.
- **BVH:** bounding volume hierarchy: a tree whose parent volumes conservatively
  contain the child volumes. A traversal can reject whole regions at once.
- **DBVT:** dynamic bounding volume tree; Bullet's real broadphase maintains
  tree nodes/proxies as shapes move. It can preserve the tree across updates.
- **Broad phase:** inexpensive conservative spatial pair filter. It must
  not confuse possible overlap with verified physical contact.
- **Candidate pair:** stable IDs of primitives whose bounds may overlap
  under the caller's query contract. A candidate is not a contact.
- **False positive:** returned candidate that fails the later precise query.
  It is safe but wastes precise-test time.
- **False negative / miss:** required pair not returned. This violates the
  conservative contract and may lead to unphysical crossings or lost forces.
- **Narrow phase:** primitive-specific precise geometry, e.g. closest-point
  sphere–triangle distance or edge–edge closest approach. Not implemented
  by the shared broad-phase service.
- **Solver:** computes the physical response; e.g. XPBD constraint projection
  or an IPC-style barrier optimization. Not interchangeable merely because
  candidate IDs have the same shape.
- **Contact manifold:** multiple actual contact points/normals used by
  contact handling; not equivalent to broad-phase candidate pairs.
- **Collision island:** a graph-connected component of bodies linked by
  confirmed contacts or constraints. Broad-phase candidates alone do not
  make valid physical islands; cloth internal constraints also couple nodes.
- **Proximity margin (\`delta\`):** caller-specified distance within which
  primitives must be candidates. In this prototype every AABB is padded by
  half \`delta\` along each axis.
- **Displacement budget (\`b\`):** padding of the stored movable *envelope*
  at cache construction, allowing some future motion before a new build.
  This is **not** an allowed contact penetration or an ignored movement.
- **Guard / envelope (\`E_i\`):** expanded bounding box cached for primitive
  \`i\`; current padded bounds must fit inside it.
- **Containment certificate:** exhaustive check \`B_i(t) ⊆ E_i\` for *every*
  primitive, in addition to stable IDs, metadata, backend and query settings.
- **Candidate cache:** a sorted, deduplicated vector of ID pairs generated
  from stored guard envelopes. Cache reuse does not rerun SAP; it does still
  validate and build current AABBs.
- **Cache hit:** certification passes; return previous pair list.
- **Cache miss:** request changed, geometry escaped, topology changed, backend
  changed, or no valid cache exists. Rebuild before returning pairs.
- **Rebuild:** construct guarded bounds and regenerate possible pairs.
- **Tree refit/update:** separately, a BVH algorithm may preserve its tree
  structure while updating moving proxies. That is the dynamic Bullet baseline.
- **Brute-force AABB oracle:** enumerate all eligible unordered primitive
  pairs and test AABB overlap. An answer is the exact *AABB candidate set*.
- **Precise geometry oracle:** enumerate every sphere–triangle pair and
  compute exact closest distance, independent from the production candidate
  loop. It is a different oracle from brute-force AABBs.
- **Conservative superset:** a returned candidate list containing every
  required pair, possibly with extra pairs.
- **Timings:** \`broadMs\` is candidate-service time, \`narrowMs\` is timed
  candidate-only precise path + contact projection, \`diagnosticMs\` is the
  final global visualization scan, \`otherMs\` includes the remainder of XPBD,
  integration, bounds generation and bookkeeping. These are CPU-side.
- **p95:** 95th-percentile observed duration (95% of measured durations are
  at or below this threshold); it is not the maximum.
- **CCD:** continuous collision detection over a time interval; a swept AABB
  is a conservative preliminary filter, not itself a proof of no collision.
- **Oracle off / unchecked:** saves validation cost but makes no zero-miss
  guarantee for the unchecked frame. Our timed benchmark is preceded by
  a separate fully checked preflight.

## Why half-margin padding is conservative

Suppose geometric shapes A and B have two points at Euclidean distance at
most delta. Their coordinate differences on every axis are also <= delta.
Therefore expanding each shape's raw AABB by delta/2 on both sides ensures
the expanded intervals overlap on each axis. The result may return false
positives, but will not omit such distance-near pairs if raw AABBs truly
contain their primitives and numeric assumptions hold.

The native query uses \`delta=0.05\` and default \`b=0.04\`.

## The enclosure proof

Let \`A_i(t)\` be a primitive's raw AABB in its current configuration,
\`B_i(t)=A_i(t)⊕[-delta/2,delta/2]^3\` its padded current AABB, and
\`E_i=B_i(t0)⊕[-b,b]^3\` the cached envelope for a movable primitive.

Cache reuse checks \`B_i(t) ⊆ E_i\` for every i, with identical eligibility
and query semantics. If \`B_i(t)∩B_j(t)\` is nonempty, pick a point x in the
intersection. Containment gives x in both \`E_i\` and \`E_j\`. Consequently

    CURRENT REQUIRED AABB OVERLAPS ⊆ CACHED ENVELOPE OVERLAPS.

This certificate concerns **discrete current bounds**. It does NOT guarantee
the collision-free path between solver states, time-of-impact, or contact
resolution correctness. The proof is conditional on valid conservative
bounds, correct metadata/eligibility and valid numeric comparisons.

## The 2 × 2 (and external BVH) experiment

There are TWO independent decisions:

**Spatial algorithm**
- Exhaustive AABB: an exact and simple pair oracle; O(n²) eligible enumeration.
- Sweep-and-prune: sort by x-min and stop an inner traversal when later
  boxes cannot overlap along x. May also degrade to O(n²) on dense scenes.
- Bullet btDbvtBroadphase: an upstream, real 3D dynamic BVH broadphase,
  not our own imitation and not a claim about the world's best method.

**Temporal update strategy**
- Rebuild the pair list on every query.
- Guard and reuse a conservative *candidate-pair list* while certified.
- Persistent Bullet DBVT update: reuse the tree while updating each AABB
  proxy. Not the same as reusing a previous pair list.

The independent leaderboard times all five choices on the exact same input
bounds per scene: exhaustive AABB, SAP rebuild, fresh Bullet DBVT, persistent
Bullet DBVT updates, guarded SAP.

**The critical distinction:** fresh DBVT is an instructive reconstruction
control; persistent DBVT is the fairer competitor for a temporal cache.
Each outputs at least the exact exhaustive current AABB pairs.
AABB-pair candidate count, false-positive inflation, builds and updates
are reported because a very cheap lookup may return an enormous pair list.

## What physical cloth was actually simulated?

The native C++ solver uses a 25×25 cloth grid = 625 particles, subdivided into
2×24×24 = 1,152 triangles, and one stationary sphere. Each 1/120-second frame:

1. Predict particle positions from previous positions plus gravity.
2. Run eight XPBD-style iterations of cloth distance constraints.
3. Enforce floor z >= thickness as a separate analytic rule.
4. Build current triangle and sphere AABBs.
5. Query the shared service, which may rebuild or certify pair reuse.
6. For returned triangle IDs only, calculate closest sphere–triangle distance.
7. Apply simple positional contact projection if within response thickness.
8. After iterations, do a final candidate query and expensive full-triangle
   **diagnostic** scan for rendering and scene-wide min gap.

The solver is independent C++ example code, not the official XPBD library
and not CCFR, IPC, OGC or cloth self-collision.

The browser rigid-sphere demo uses an entirely separate impulse solver and
contact-island/analytic sphere TOI path. Its results do not prove shared-cloth
self-collision, cross-method speedups, or an all-purpose solver.

## Why not three cloths interacting?

Multi-cloth BROAD PHASE is meaningful as a geometric stress test because it
creates more possible primitive pairs, rapidly changing topology of *potential
contacts*, dense overlapping bounds, and worse candidate inflation.

The current 2/3-sheet leaderboard cases have many triangles from distinct
geometric sheets and two spheres. They are **synthetically animated 3D AABB
workloads**; no coupled cloth response is simulated. They intentionally
exercise geometric pair filtering without pretending the narrow phases or
solvers are complete. This keeps this study broad-phase-focused.

Real physically interacting cloths would additionally require:
- exact vertex–triangle and edge–edge tests, contact exclusion for incident
  mesh primitives and other adjacency rules;
- stable body/cloth/primitive IDs, topology changes and broad-phase updates;
- method-specific contact thickness, repulsion, friction and stiffness;
- potentially continuous collision constraints or motion bounds;
- numerical acceptance, mesh intersections and convergence tests.

Those are necessary to claim a *physics solver demonstration*. They are NOT
required to compare conservative 3D broad-phase candidate generators on
recorded mesh trajectories. The next scientifically clean test is to replay
trajectories from the SAME reference cloth solver through different broad
phases, holding the downstream contact predicates and solver unchanged.

## The measured cascade: when does reuse improve total work?

An end-to-end model is:

    T_step =
      T_geometry_bounds
      + T_broad_phase
      + N_candidates * T_precise_average
      + T_solver
      + T_diagnostics / T_other

The actual precise-test cost is variable, so that product is only a rough
model. The code now records precise-test counts and separately measured
pipeline stages rather than **inventing** a downstream speedup.

The guarded policy saves reconstruction work but may return more candidates.
If extra precise work exceeds saved builds, total contact handling can get
slower, even while the broad-phase bar becomes dramatically shorter.

For a simplified one-query threshold:

    benefit ≈ (T_build - T_certificate) * P(reuse)
              - (N_guarded - N_fresh) * T_precise

Positive benefit is a *hypothesis about a particular workload*, not a
theorem. This is where an adaptive envelope budget could become a research
topic: choose a safe budget to optimize **total** collision-stage cost, not
only maximum cache hits. This adaptive selection is NOT implemented.

## Proven facts, hypotheses, and novelty

**Verified in current prototype**
- Stable, deterministic, sorted candidate IDs and eligibility masks.
- SAP agrees with exhaustive AABB candidates on tested fixtures.
- Guarded envelopes cover every current AABB candidate in tested fixtures
  when enclosure conditions hold.
- An independent exact sphere–triangle oracle checks the live cloth fixture.
- Paired rebuild/guarded tests use the same solver and compare all particle
  coordinates; saved runs reported no numerical divergence.
- Genuine Bullet DBVT comparisons on identical 3D synthetic inputs.
- Runtime logs expose cache reasons; viewer shows real recorded metrics.

**NOT a new contribution by itself**
- AABB geometry, SAP, dynamic BVHs and their insertion/refit strategies.
- Larger/fat AABBs or temporal candidate caching.
- A simple static-geometry cloth falling on a sphere.
- Proof that nested spatial envelopes generate nested overlap candidate sets.

**Potential future research novelty (unverified)**
1. Define a *query contract* that specifies primitive classes,
   proximity/offset, continuous motion envelope, pair eligibility,
   correctness oracle, geometry version and tolerances.
2. Provide a rigorous certificate for reusing candidate lists under that
   contract across **different** collision formulations.
3. Optimize the envelope budget by measuring saved builds versus downstream
   false-positive precise-query cost.
4. Validate against real source-level adapters to IPC/OGC/XPBD/CCFR,
   preserving those methods' own narrow phases and solvers.
5. Demonstrate robustness and benefits on demanding recorded scenes:
   1/2/3+ interacting cloth sheets, collisions among sheets, dense folding,
   motion speed, GPU/CPU differences, and nonpenetration criteria.

Until (1)–(4) are actually built and evaluated, call this an **experimental
architecture and a working proof-of-concept**, not a newly established
research algorithm.

## What the live evidence viewer means

After running \`./scripts/prepare_meeting.sh\`, open \`./open_3d_demo.sh\`.
The main cloth and sphere are real simulation state in the same C++ process.

- \`1\`: geometry.
- \`2\`: certified bounds.
- \`3\`: amber broad-phase candidate triangles.
- \`4\`: green exact proximity triangles.
- \`Q\`: rebuild versus guarded pair-list reuse; compare work counters.
- \`M\`: change margin, forcing cache invalidation.
- \`B\`: SAP or exhaustive AABB baseline.
- \`V\`: independent precise oracle on/off (unchecked is NOT zero misses).
- \`N\`: advance a single physics frame; \`SPACE\` pause.
- \`G\`: toggle the **new right-side evidence inspector**.
- \`H\`: cycle external 3D benchmark scenes.
- Scroll over the right-side inspector to see event-log, paired cloth CPU
  step chart, 5-way published-library AABB leaderboard, actual downstream
  measured cascade and the bounded novelty statement. Scroll on the 3D
  viewport to zoom.

The evidence pane reads only local CSV files generated by the current
checkout. If data is missing, it clearly says so rather than inventing
numbers. Look under \`output/native/\` for SVG figures and raw CSVs.

## Suggested meeting explanation

> We first separate broad-phase candidate generation from the narrow-phase
> contact predicates and physical solver. We currently use stable primitive
> IDs, conservative AABBs and bilateral eligibility filters. Our question
> is whether repeated candidate-generation work across solver iterations can
> be avoided safely. We cache pair lists from displacement envelopes and
> certify on every query that every current padded primitive bound remains
> enclosed. This makes cached candidates a conservative superset of fresh
> current AABB-overlap candidates. We compare against exhaustive AABB, local
> sweep-and-prune, and the actual upstream Bullet 3D dynamic BVH, including
> persistent tree updates. We separately measure downstream exact tests and
> physical-step time, because extra cached candidates have costs. Our current
> XPBD-style cloth test is deliberately limited; the research goal is to
> define the correct caller-specific contract for a real IPC, OGC or
> CCFR/XPBD adapter and evaluate whether a shared service is genuinely useful.

## Primary implementation and literature references

- \`include/ccp/live_cloth.hpp\`: bounds, request, service, result and metrics.
- \`src/live_cloth.cpp\`: AABB, SAP, oracle, guard certificate and cloth adapter.
- \`apps/broadphase_leaderboard.cpp\`: actual Bullet reference and equal-input
  benchmark, without a solver.
- \`apps/live_benchmark.cpp\`: real physical-step and precise-query effect.
- \`native/main.cpp\`: native 3D scientific presentation UI.
- Bullet upstream: https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/BroadphaseCollision/btDbvtBroadphase.h
- Box2D dynamic tree (related design, *not* the measured 3D implementation):
  https://box2d.org/documentation/group__tree.html
- Karras, HPG 2012 GPU LBVH (*not* measured in this CPU benchmark):
  https://research.nvidia.com/publication/2012-06_maximizing-parallelism-construction-bvhs-octrees-and-k-d-trees
- Cohen et al., I-COLLIDE, sweep-and-prune antecedents:
  https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/cohen95.pdf
- IPC: https://ipc-sim.github.io/
- OGC: https://ankachan.github.io/Projects/OGC/
- PBD source: https://github.com/InteractiveComputerGraphics/PositionBasedDynamics
- CCFR: https://arxiv.org/abs/2607.26901


## Moving multi-cloth research experiment

A live native C++ simulator now drives one, two or three deformable cloth sheets with two moving spheres and discrete inter-sheet contact corrections. The regional-certified candidate service caches pair blocks for stable mesh patches, selectively invalidates blocks touching escaped regions, and rechecks every cached candidate against current tight padded AABBs. Its adaptive budget uses measured motion and rejected-pair ratio. This is an experimental combination, NOT established novel prior art: PhysX has region broad phases and 2026 research already covers selective deforming BVH updates.

The new simulated_cloths3 external benchmark records real physics-generated geometry and compares upstream Bullet DBVT rebuild and persistent update, SAP, global guard, and regional fixed/adaptive strategies with the same AABB masks and positions. Read raw candidate counts and full solver comparisons before drawing conclusions.
