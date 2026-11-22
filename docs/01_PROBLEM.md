# Spacetime Contact Atlas: one precise collision research problem

Status: HYPOTHESIS. No novel algorithm, cross-solver speedup, or CCD guarantee has yet been established for this new branch.

## Why this problem exists

In highly deforming 3D scenes, a contact solver needs to know which geometric features could meet within the next continuous motion interval. Testing every triangle against every other triangle is quadratic. A maintained collision structure can reject large spatially separated groups, but refitting/updating it and producing candidates may itself dominate when local contact regions change rapidly.

Our niche is **local, rapidly changing contact on moving triangle surfaces**: self-collision in folding deformable surfaces plus interactions with other deformable/rigid meshes. Primary comparisons use true contact-rich scenes with real solver trajectories and the same narrow phase. Particle and volumetric extensions cannot count toward the core result until correctly adapted.

**Research question:** Can a correctness-preserving locally updated swept-contact hierarchy reduce fully paid contact-detection work over a tuned published LBVH and fixed SAP, without skipping any solver-relevant continuous candidate, for contact-localized mixed triangle-mesh scenes?

This is not a new physics engine, a universal common-contact API, a graphics renderer, or an ML controller.

## Exact mathematical contract

For a triangle feature i, let its moving vertices be x[i,k](s), with normalized step time s in [0,1]. If each vertex moves on a LINEAR path,

    x[i,k](s) = (1-s) x[i,k](0) + s x[i,k](1).

With approved nonnegative proximity thickness h[i], a conservative swept bound is

    S[i] = AABB({x[i,k](0), x[i,k](1) for every k})
           ⊕ [-h[i],h[i]]^3.

This enclosure is valid for linear paths because every intermediate vertex lies in the convex hull of its two endpoints. It is NOT generally valid for rotations represented as curved vertex paths, nonlinear interpolation or unbounded mid-step solver iterations. Those require certified path bounds/subdivision.

Let A(i,j) be the solver-approved compatibility and adjacency predicate. Define the exact swept-AABB BROAD-PHASE reference candidate set

    B = { (i,j) : i<j, A(i,j), S[i] intersects S[j] }.

The proposed accelerator should enumerate exactly B, with unique stable sorted IDs, or announce an explicitly defined conservative superset whose extra work is measured.

**This is distinct from true continuous contact**:

    E = { (i,j) : exists s in [0,1] such that
          distance(feature_i(s), feature_j(s)) <= h[i]+h[j] }.

The same step time s matters. Swept AABBs can overlap even if moving features never occupy the same place at the same time. A CCD algorithm, not an AABB overlap, is required to establish an event in E. If the motion bounds and filters are faithful, E is a subset of B. Missed B pairs are disallowed; missing E events is a separate safety failure.

The measured objective is

    T_paid = T_bounds + T_invalidation + T_structure_maintenance
           + T_pair_enumeration + T_normalization
           + T_narrow_phase + T_contact_pipeline

and, independently, total physics step time. The renderer is excluded from physics timing. Report both CPU time and wall-clock latency where relevant.

## Proposed method: provisional Spacetime Contact Atlas

1. Accept solver-native vertex trajectories and certify their complete motion envelopes. Reject invalid/nonfinite states, inverted bounds, duplicate IDs, and unsupported nonlinear paths.
2. Maintain a versioned swept-box hierarchy and a spatial contact frontier. Only geometrically dirty leaves and affected ancestors are eligible for local maintenance; stale descendants must not survive invalidation.
3. Choose local refit, subtree rebuild, or full rebuild using *measured* maintenance cost and occupancy. This controller cannot remove a candidate or alter the truth predicate.
4. Traverse all overlapping subtrees, including distant-in-topology folded mesh regions, mixed-object contacts, and inter-region pairs. Produce the same B as the independent reference.
5. Pass exactly the same candidates to pinned external CCD and unchanged physics integrations. Require complete geometry/state equivalence within documented numerical tolerances.
6. If a certificate or version is invalid, fall back to a known-correct recomputation. Count the cost and frequency.

This architecture has substantial prior art. A research contribution would need a *specific differentiating update rule, mathematical argument, cost model or new validated regime* beyond local refitting/front tracking. The title alone is not novelty.

## Accepted invariants

- Collision masks and adjacency rules are separate; do not ignore self-contacts solely because primitives share an object.
- Every control comparison shares geometry, solver, thickness, timestep, initial state and feature filters.
- A result against a quadratic reference alone is insufficient; official IPC Toolkit LBVH is mandatory.
- Every incorrect pair, invalid physical state, penetration/tunnelling case, and worst-tail regression must remain published.
- Continuous correctness must be checked with an independent continuous method; sampled frame audits alone cannot prove it.
- If broad-phase candidates are identical, a different-looking deformation is a bug, a different solver, or a visualization effect, not an algorithmic advantage.

## Scope of intended results, not claims of success

Target at least 1.20x equal-scene geometric-mean total collision-detection speedup versus the best fixed deployable comparator, with scene-clustered bootstrap 95% CI entirely above 1.0. Seek 1.10x full solver-step speedup only where contact detection is a material share of the step. No missed exact candidates, no observed independent CCD misses, no new unresolved geometry invalidity, and no hidden catastrophic tail. Two solver integrations and one external benchmark are requirements before cross-solver superiority can be claimed.

A failed target means the method is not established, not that plots should be selectively redrawn.
