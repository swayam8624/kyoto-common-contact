# Worked laboratory: from current cloth boxes to correct candidate pairs and bounded contact response

The key source files are [live_cloth.cpp](../src/live_cloth.cpp), [research_multicloth.cpp](../src/research_multicloth.cpp), [online_partitioner.cpp](../src/online_partitioner.cpp), [cloth_interaction.hpp](../include/ccp/cloth_interaction.hpp), and [research_multicloth_tests.cpp](../tests/research_multicloth_tests.cpp). For the architectural reading itinerary use [SOURCE_READING_ORDER.md](SOURCE_READING_ORDER.md).

This is an experiment in *conservative broad-phase collision candidate generation*. It does not make a continuous collision detection (CCD), physically exact contact or universally faster solver claim.

## 1. Start with the physical problem

MultiCloth simulates one to three 13x13-vertex cloth sheets with two triangles per 12x12 cell, plus moving sphere obstacles. Each physical step uses a fixed dt of 1/90 second and five XPBD-style constraint/contact iterations. A triangle's position changes as vertices move and fold, so its current axis-aligned bounding box must be reconstructed from the current three positions.

The broad phase asks: which unordered pairs of these primitive AABBs are eligible under collision masks and overlap after the configured margin? Every such pair must be included in the returned candidate set if the algorithm claims full *discrete current-AABB coverage*.

The narrow phase asks a different question: which vertices, faces, edges or sphere surfaces are actually close enough to need positional response? A candidate may turn out to be a false positive. A broad-phase overlap does not mean the two triangles contact.

## 2. Derive the box-overlap test

For two boxes A and B, inclusive overlap on X means A.minX <= B.maxX and B.minX <= A.maxX. The same condition is necessary on Y and Z, so the full 3D test is the logical AND of the six scalar comparisons.

A primitive also stores category and mask bits. Even overlapping geometry may be ineligible—for example, a same-sheet adjacent triangle pair excluded by a topology policy.

**Concrete example:** A covers X=[0,2], Y=[0,1], Z=[0,1]. B covers X=[1,3], Y=[0.5,1.5], Z=[0,1]. They overlap. C covers X=[2.2,4], Y=[0,1], Z=[0,1]. A and C do not overlap in X despite the other axes overlapping.

The project pads raw bounds by margin/2 on each side before a broad-phase query. With margin 0.4, A extends to X=[-0.2,2.2] and C to X=[2.0,4.2], so padded boxes overlap and become candidates. That is conservative proximity, *not penetration*.

**Exercise:** take four AABBs, compute all six-axis comparisons manually and record the canonical pair IDs (min ID, max ID). Then run live::candidates with useSAP=false as your independent current-AABB oracle.

## 3. Understand why SAP can stop early

Sweep and prune sorts boxes by minimum X. For a box A, it scans later boxes B until B.minX exceeds A.maxX. Since the rest of the list has minX no smaller than B.minX, none of those later boxes can overlap A in X. Breaking the loop is therefore correct for that static sorted list.

The algorithm still checks collision eligibility and Y/Z overlap for boxes whose X intervals intersect. A sorted sweep can avoid many unnecessary AABB comparisons in sparse scenes, but in all-overlapping scenes its comparisons can approach the brute-force quadratic count.

**Exercise:** sort the boxes above by minX and mark the first candidate that terminates A's inner sweep. Contrast *pair tests* with the final *candidate pair count*. A low query time with missing pairs is not a successful optimization.

## 4. Why a fat guard can be reused

BroadPhaseService stores a padded/fat AABB E_i for each primitive and a list of pairs built from those envelopes. For a later frame at time t, let the true current padded box be B_i(t).

The vital invariant is: for every i, B_i(t) is a subset of E_i, with unchanged primitive identity, category/mask/mobility and query policy.

Suppose current B_i(t) overlaps B_j(t). Because each current box lies inside its respective envelope, their envelopes E_i and E_j must also overlap. Therefore that pair must have been a candidate when the envelope pair list was built (assuming same eligibility rules). The cached pair list is a **superset** of possible current overlaps.

This is the logic behind *certified reuse*. It does not mean every cached pair currently overlaps. A temporary moving-object separation can leave an obsolete pair in the list, and only a current-tight-bounds recheck can remove that false positive safely.

**Worked example:** a box moves within a 0.1-unit outer envelope. The stored E_i still contains it, so the cache is valid. When it moves 0.11 units outside the envelope, even if no visible contact has changed, the cache must be rebuilt; otherwise a new overlap outside the old search envelope could be missed.

**Exercise:** move a primitive incrementally inside and beyond its guard, and compare the returned candidate set against brute force every step. Record whether the service reports reuse versus rebuild. Verify that changing category/mask is treated as metadata invalidation too.

## 5. Why regional caching needs two tiers

RegionalCandidates groups primitives into spatially informed *topological patches*: each cloth triangle belongs to a mesh-cell patch, with moving spheres in separate regions. A region maintains per-primitive guard envelopes plus a union envelope.

Tier one finds overlapping region-union bounds. Hierarchical mode sweeps region envelopes, while flat mode enumerates region pairs. Disjoint union envelopes cannot contain an overlapping primitive pair, so pruning them is safe if all individual current padded bounds remain within their guards.

Tier two enumerates or reuses primitive pairs for each active region pair. If a region's member exceeds its guard, the region becomes dirty and affected pair blocks rebuild. Even reused pair blocks are filtered against **current padded AABBs** and collision masks before being returned.

A regional cache requires correct *identity and membership*, not just approximately similar geometry. If a triangle changes patch ID after split/merge, stale member arrays could map cached pairs to the wrong object; the code resets state on topology signatures.

The current regional code stores per-region motion EWMAs and rejected-pair inflation estimates, using them to adjust guard budgets. A smaller guard reduces stale candidates but rebuilds more often; a larger guard increases reuse but may enlarge narrow-phase work. The choice is a performance tradeoff, not a change in which current contacts are physically required.

## 6. Work through an online patch decision

OnlinePartitioner starts from 6x6 patches over the 12x12 cell lattice. It measures per-patch quantities rather than using future-contact oracle labels.

The proxy score is:

W = AABB_checks + 0.65 * cached_rechecks + 2.0 * rejected_candidates + 3.0 * invalidations.

Imagine a patch reports 100 AABB checks, 40 cached checks, 10 rejects and 2 invalidations. Its measured work score is 100 + 26 + 20 + 6 = 152. That is a heuristic work score, **not 152 milliseconds**.

With smoothing beta 0.22, an old EWMA of 100 updates to 0.78*100 + 0.22*152 = 111.44. The controller also maintains contact and candidate EWMAs. A hot classification requires both enough work and suitable contact activity; a quiet classification requires low work and low contact.

Every six frames, hot/quiet windows are advanced. Two hot windows can permit a split, and all four siblings must be quiet for three windows before a merge. There is a 12-frame patch-edit cooldown. A separate 18-frame cooldown gates flat/hierarchical traversal switching.

**Exercise:** feed a sequence of six-frame window summaries with work scores 200, 210, then 20, 10, 10 and varying contact counts. Identify when a patch becomes eligible for split and why a subsequent merge needs four separately quiet siblings. Compute EWMAs instead of assuming instant threshold crossings.

## 7. Repartitioning does not remesh the cloth

OnlinePartitioner::split replaces one parent patch with four half-span children. The cell membership changes, but the triangle vertices, constraints and rest lengths do not. After the controller decides to change the partition at the end of MultiCloth::step, assignOnlineRegionIds remaps triangle region keys and resets RegionalCandidates.

This means two algorithms can evolve the exact same physical mesh trajectory while using different candidate-maintenance structures. It also means that pure patch-count changes are not evidence of better physical behavior or faster total solver performance.

A mode switch is chosen from smoothed observed timings collected on different frames. It cannot be described as a controlled same-state A/B measurement. The independent benchmark harness must compare both algorithms on identical input snapshots for that claim.

## 8. The narrow phase has its own failure modes

projectContacts handles triangle-sphere distances and inter-sheet or nonlocal same-sheet triangle features. For triangle pairs it considers vertex-face and edge-edge closest features; an extra segment-face piercing check helps detect some discrete self-fold intersections.

Each resulting positional correction is weighted by inverse mass and capped per particle per iteration. Otherwise one vertex shared by many candidate triangles could accumulate dozens of large corrections and acquire enormous artificial next-step velocity in the Verlet formulation.

The contact budget and post-step previous-position damping reduce instability, but are approximations. This is **not a barrier formulation** and **not an intersection-free CCD proof**. Fast objects could cross between discrete solver times, and tangled folds can remain.

**Exercise:** make a deliberately folded self-contact fixture and record candidate count, selfPreciseTests, selfCrossings, selfContacts, maximal frame particle travel and structural stretch. Compare with self-contact disabled. A zero oracle miss count alone does not guarantee that the cloth looks stable.

## 9. One drag from screen to a physical constraint

The native GLFW viewer reads cursor coordinates in *window units*. cameraRay converts those into a perspective world ray; Retina framebuffer dimensions must not be mixed with window cursor coordinates.

pickTriangleRay performs two-sided Moller-Trumbore ray tests and selects the nearest positive hit, producing triangle ID and barycentric weights. beginGrab validates those weights and verifies a movable attachment; later mouse movement is intersected with the camera-facing plane through the picked point.

projectGrab applies a bounded mass-weighted positional constraint inside the same solver iterations as cloth structural and collision constraints. Dragging is neither direct triangle teleportation nor broad-phase optimization; it changes the geometry on which broad phase operates.

**Exercise:** choose barycentric weights (0.2,0.3,0.5) for a three-vertex triangle and compute the picked position as their weighted sum. Then drag the target upward while keeping the camera ray parallel to the intended plane and confirm that a parallel-plane intersection returns no target.

## 10. A complete correctness and performance experiment

For a fixed scene seed, reset two MultiCloth instances to identical positions and constraints, one using RebuildSAP and one using OnlineContactAware. At selected frames and solver iterations, compute brute-force current padded AABB oracle pairs. The online candidate set must include every oracle pair; for the exact-current regional query, it should agree when its contract demands equality.

Then compare actual particle positions and material/constraint states, maximal travel, self-contact statistics, contact correction counts and total step time, not only query time. Account for oracle audit overhead, controller decision time, warm-up and native renderer overhead separately.

Use [tests/research_multicloth_tests.cpp](../tests/research_multicloth_tests.cpp), [apps/online_controller_trace.cpp](../apps/online_controller_trace.cpp), [apps/multicloth_benchmark.cpp](../apps/multicloth_benchmark.cpp) and [docs/EVIDENCE_METHODS_AUDIT.md](EVIDENCE_METHODS_AUDIT.md).

For a comparison with Bullet DBVT, require the external Bullet library target and label **isolated broad-phase query** timings separately from **full coupled physics step** timings. A benchmark on one fixture does not establish universal superiority or unpublished novelty.

*Source-derived teaching and worked arithmetic. No new C++ compilation, physics benchmark, screenshot or native viewer run was performed here.*
