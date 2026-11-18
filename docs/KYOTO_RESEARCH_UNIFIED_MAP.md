# Kyoto research: the entire algorithm map (and what Phase 17 proves)

This is an orientation to the existing experiments, **not** a replacement for
the historical documents or the frozen V4 release. All original code and
negative results remain on their original branches.

## 1. The problem and its boundaries

Elastic/deformable objects move and collide. A physics pipeline needs to
locate pairs of primitives that might intersect. The broad phase answers
**which pairs of current conservative axis-aligned bounding boxes overlap**.

That is only one stage. The narrow phase computes more precise geometric
contacts; continuous collision detection (CCD) considers motions between
samples; the constraint solver resolves penetration and computes updated
positions/velocities. Graphics shows the solved mesh.

Two exact broad phases returning the same pair list to the same solver
should yield the same physical outcome. A 5x faster broad-phase query is not
a 5x faster whole simulation, and it cannot independently reduce folding.

A "candidate" is not yet a real triangle collision, and one candidate pair
can be encountered at many different frames.

## 2. The original research track: an Online broad-phase controller

The original Kyoto question was not "make a hash grid with more threads."
It was "can we make an exact, work-aware, causal broad phase handle changing
deformable contact better than a static global choice?"

The historical approach families and where they remain:

| Family | Source | What it tests |
|---|---|---|
| Rebuilt SAP | material replay / live candidates | Independently sorted x-axis sweep, exact baseline |
| Persistent SAP | final_broadphase.hpp | Retain and repair previous x ordering |
| Global Guard | material replay / live BroadPhaseService | Cached protected candidate envelopes, recheck current boxes |
| Selective Guard | selective_guard.hpp | Partial exact cache repairs |
| Online | generic_online.hpp, ONLINE_ALGORITHM.md | Causal work-aware regional selection |
| Learned Online | learned_online.hpp, learned_policy.hpp | Learned backend *cost*, never learned collision pairs |
| Guard/SAP measured probe | material replay | Charge both backend trials when probing |
| Guard/SAP bounded trial V4 | final_broadphase.hpp | Pay for only one backend each frame, bounded SAP trial |
| Regional exhaustive / adaptive / hierarchical | regional candidates | Different partition/caching decisions |
| Bullet DBVT | conditional in material replay | An external published broad-phase family, when linked |

The ML selector uses a 12-feature supervised model. Its labels predict the
cost of independently maintained teacher policies, not the truth of whether
objects collide. A switched controller pays for rebuilds/cold caches.
The project did **not** prove universal ML superiority. The native .mlpc
weight artifact is not tracked on the current active branch. The Phase 17
dashboard therefore displays ML only when the caller actually supplies
compatible weights; otherwise it explicitly says ML was not executed.

The original work included controlled ablations and several failed proposals.
A failed experiment is still evidence. Do not delete it to make a prettier
leaderboard.

## 3. Why V4 was frozen

The V5 864-scenario benchmark and the independent V6 864-scenario confirmation
favored the V4 Guard/SAP bounded trial against Guard under those synthetic
distributions. The documented scene-level geomean Guard/V4 ratios were
1.091216x (V5) and 1.083201x (V6); summed-scene means gave 1.328684x and
1.316721x respectively. This does not imply the controller wins on every
scene, a complete cloth-solver speedup, or a graphics novelty claim.

The V6 unified certified hierarchy lost overall to Guard:
approximately 0.629685x geometric speedup. It was not promoted.

The frozen engineering interface lives in final_broadphase.hpp. Do not retune
it on data already inspected and then call that a held-out validation.

## 4. A second track: regions, scene structure, and million-box workloads

After V4, a series of experiments explored *why* it could be slow in other
regimes and how to build a more scalable representation.

Phases 0–4 studied cross-region search, certified caches, SharedBVH and a
cautious controller selecting between V4 and regional methods. They revealed
both potential wins and the real cost of switching and cache invalidation.

Phase 5 exercised real coupled-cloth geometry (exported snapshots) with
different topology patch spans. Naive regional methods lost to frozen V4 on
this controlled 577-primitive sequence. It was an important negative result.

Phase 6 added a safe x-axis prune of region envelope pairs. On a very fine
patch partition it reduced cross-region tests markedly, but the whole
method was not universally faster than V4.

Phase 7 examined many independent, physically stepped material worlds.
The CPU comparator used their exact same recorded AABB data. World-to-world
collision constraints were not solved. An observed 1000-world contact_front
case strongly favored regional X sweep over frozen V4, but that was one
synthetic-generator scene, not a validated deployable universal win.

Phases 8–10 developed real-glTF replay and a four-arm on-policy portfolio.
A 120-frame real-geometry replay showed the portfolio improving over frozen
V4 but losing to fixed SharedBVH. Selecting a good arm online still costs
cold starts, trials and migration. Hindsight-best is not a causal controller.

Phases 11–13 started a separate **global pressure** family: sparse, slabs,
burst, teleport, bridge and all-overlap. These operate on synthetic current
AABBs at scales up to one million or two million objects. In most of these
experiments the result is just candidate count plus commutative checksums.
A Phase 11 independent rebuilt-grid command also streamed actual pair IDs.

Phase 14 tried incremental occupancy stored in a pointer-heavy hash map.
Correctness held, but measured memory locality and enumeration performance
were poor. Phase 15 replaced the query-facing occupancy with compact bins
while retaining a map only for updates. Phase 16 ran the exact same
persistent-bin pair comparisons across multiple CPU cores. Your Mac's
1-million-burst warm full-query 1-to-8-thread improvement was approximately
5.191x, versus 1.645x for quiet slabs. Those were **not** full pair emission
times or whole-solver times.

No earlier algorithm was removed by this progression. The publications,
experiments, output contracts and scene generators simply diverged.

## 5. What Phase 17 adds on its own research branch

Branch: research/kyoto-phase17-unified-proof

1. The existing material replay runs all historical full-output CPU methods
   on exactly the same exported material AABBs (and ML/Bullet if available).
   The strict scorer rejects disagreement with rebuilt SAP on every frame.
   The new dashboard reports original methods together, relative to
   Global Guard, with trajectory-level aggregation.

2. A new native persistent-grid API, FlatPersistentGrid::streamPairs,
   emits actual canonical current-AABB pairs through a bounded callback.
   The new ccp_cbp_pressure_incremental_flat_emit executable writes
   8-byte little-endian ordered IDs in chunks, charging writes and
   persistent index maintenance, refusing overwrites and discarding
   incomplete temporary files.

3. The older independent rebuilt-grid pair emitter runs on the **same**
   synthetic scene, object count, seed and frame count.

4. A Python independent verifier reads every pair from actual binary
   outputs, recalculates the 64-bit and 32-bit digests, checks canonical IDs
   and duplicate pairs via external sorting, checks pair bytes, and compares
   the sorted complete pair ID sets emitted by the persistent and rebuilt
   implementations. This is stronger than matching only counts and hashes.

5. Regression tests reject invalid IDs, duplicate or truncated binary
   records, changed hashes, missing historical methods and misaligned
   synthetic cohorts. Linux CI runs a reduced proof set before the Mac
   benchmark. A CI pass does not establish Mac speedup statistics.

The unified report has **two tables**, not one deceptive rank list.
Original methods use FULL output and material frames; the Phase 16
comparators use synthetic count/digests. Timing is compared only within
each domain. A checked pair-file proof does not make the algorithms' two
full query timings comparable unless exactly equivalent preparation and
sink costs are controlled.

## 6. Reproduction on the original Mac

Use a fresh output folder. This does not delete previous results:

    cd ~/Desktop/Programming/kyoto-common-contact
    git fetch origin
    git switch research/kyoto-phase17-unified-proof
    git pull --ff-only origin research/kyoto-phase17-unified-proof
    bash scripts/run_kyoto_phase17.sh output/kyoto-phase17-mac-01

When the original trained native ML artifact exists, pass it as a second
argument. Phase 17 does **not** secretly train a model, contaminate a test
split or fabricate an ML benchmark result.

The existing real solver visualization is still available independently:

    bash scripts/run_kyoto_visual_package.sh output/kyoto-showcase-fresh

## 7. What is still needed for a defensible research paper

The previous work and Phase 17 improve engineering correctness and evidence,
but **do not finish a scientific novelty claim**. That would require:

* direct integration of a candidate generator into the same coupled
  triangle-level cloth/elastic solver, validating contact behavior and CCD;
* truly independent sequences of deforming meshes with topology and
  adjacency semantics, including tearing and self-contact when supported;
* fair optimized external library comparisons (Bullet DBVT, PhysX ABP/SAP,
  IPC/LBVH where algorithms' semantics match);
* repeated held-out randomized benchmark order with robust trajectory
  confidence intervals, cold starts, total CPU/GPU host and device cost,
  full pair-output bytes and downstream solver time;
* a new mathematical or algorithmic hypothesis (for example exact
  certified interaction blocks) actually implemented, proved and tested,
  rather than presenting multicore spatial hashing as inherently novel.

Keep the frozen V4 engineering deliverable. Keep the unsuccessful arms.
Keep the Phase 0–16 evidence. The next new science should build on a
*visible, honest baseline* rather than erase the journey.

## 8. A logical study sequence once implementation is reviewed

Start with exact AABB overlap, sweep-and-prune, temporal coherence and
cached guard certificates. Then follow the Online work model, regional
decomposition, learned *cost* routing, one-backend V4 trials and held-out
validation. Next study spatial hashing, canonical ownership, output
complexity, memory locality, persistent occupancy and parallel reduction.
Finally connect these to narrow phase, CCD, solver dynamics, GPU scheduling,
real deforming meshes and formal research novelty.

This sequence explains *why each experiment was attempted, what it actually
proved, and where it failed*—instead of treating Phase numbers as separate
unrelated projects.
