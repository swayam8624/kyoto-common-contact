# SCA concrete method blueprint — falsifiable implementation target

Status: precise design hypothesis. The Stage-0 code does NOT yet implement
this structure or prove a performance advantage.

## Research hypothesis

In a geometry sequence where most moving features are spatially coherent
and true contact lies on a sparse, rapidly migrating subregion, a
versioned **node-pair contact frontier** with independently validated
dirty-propagation may reduce paid index traversal and update cost
compared with rebuilding a high-performance LBVH every query.

This hypothesis is narrow and has antecedents in kinetic collision-front
tracking. We must establish a concrete distinction or accept that our
contribution is engineering rather than novelty.

## Inputs and canonical truth

Solver emits stable primitive IDs, topology and filter epochs, exact
swept bounds S_i for the whole intended motion path (inflated by the
solver's contact radius), plus optionally a **guaranteed** changed-ID
set D. If D cannot be trusted, the implementation validates current
boxes against stored boxes and pays for that scan.

Reference B is the sorted set of all compatible primitive pairs whose
current certified swept AABBs inclusively overlap. A random query can
be checked against a quadratic independent reference on small scenes.
An external continuous CCD pipeline then distinguishes actual events.

## Hierarchy structure

Each leaf L_i stores stable ID, envelope S_i, topology/filter epoch
and monotonically increasing box version. Every internal node v stores

    U_v = hull(U_left(v), U_right(v))

plus subtree identity/epoch, box version, child IDs, leaf count and
measured query/refit counters. No internal bound may fail to contain
the current certified swept bounds of every descendant.

The contact frontier is a set of node pairs F={(u,v)} partitioning the
unexamined/overlapping cross-subtree work. A cached node pair may retain
its *separated* conclusion only if both of its bounding boxes and
compatibility-relevant epochs remain valid for the current query.
A cached overlapping conclusion still requires exact current leaf
enumeration; it must not be mistaken for physical contact.

For each new interval:

1. Validate topology/filter epochs and the solver's motion envelope.
2. Derive D from exact changes, either solver-guaranteed IDs or paid scan.
3. For each changed leaf, replace S_i, increment version, refit the
   closure of affected ancestors bottom-up.
4. Invalidate frontier records whose node-pair proof relies on changed
   box versions or topology/filter epochs. A global topology reset
   may discard the entire frontier.
5. Re-evaluate invalidated frontier node pairs using complete dual-tree
   recursion. For disjoint node boxes, reject that subtree pair.
   For overlap, descend into children; at leaves emit only compatible
   exact AABB-overlap pairs. Any subtree that was not represented by a
   provably valid cached conclusion must be traversed now.
6. Normalize pairs by stable primitive IDs, including cross-object
   and nonlocal same-object folds.
7. If an invariant or version chain fails, discard the cached frontier
   and perform full traversal or exact reference fallback.

Structural rebuild may replace a subtree for better spatial locality,
but MUST invalidate all frontier records using the changed subtree.
It can never make a previously possible pair disappear by heuristic.

## Decision rule: performance only

Let R_v be measured refit cost of region v; Q_v the paid mean query
cost for a subtree over a fixed recent training window; and B_v the
measured rebuild cost. Let estimated query benefit from rebuilding be
G_v. A provisional selector chooses rebuild only when

    H * G_v > B_v - R_v

for a pre-registered expected remaining use horizon H, and otherwise
refits. H and G_v are **empirical estimates**, not collision bounds.
No prediction may authorize dropping pairs. Report model residuals,
wrong decisions, stability, and an ablation replacing the selector
with fixed refit/rebuild. If the cost model does not help on locked
data, remove it.

Do NOT claim a competitive ratio, asymptotic local-update bound or
novelty until independent proof/specific comparison is complete.
Dense contact can force quadratic candidate output and negate any
spatial-index advantage; benchmark that lower bound openly.

## Independent safety gates for implementation

G1: all node boxes conservatively enclose their descendant leaves.
G2: every cached separated node-pair conclusion has unchanged certified
    geometry and filter epochs; changing any input invalidates it.
G3: every leaf-pair not ruled out by valid separated frontier records
    is traversed and exactly tested.
G4: output pairs are stable, sorted, unique, mask/topology compatible.
G5: external CCD event pairs are a subset of the output under
    explicitly stated, validated path-envelope assumptions.
G6: reference SAP, brute force and pinned IPC LBVH comparisons use
    semantically identical primitive sets and boxes.
G7: physics-solver trajectories remain equivalent when the same CCD
    pairs/events are consumed.

Tests must attack stationary boundary touching, certificate expiration,
stale rebuilds, simultaneous topology + mask mutation, ID remapping,
rapid reversal, tiny/huge boxes, NaNs, infinity, empty scenes, dense
all-overlap, distant-in-topology folds and cross-object contacts.

## Minimum expected deliverable

A C++23 SCA library; three reference comparators; an independently
checked real CCD adapter; deterministic scene/trajectory inputs with
versioned receipts; full paid stage traces; exactly specified testing
gate; two native solver integrations; source-backed 3D observatory.

We are NOT building the renderer first and claiming its scene is
science. The renderer consumes real output once it exists.
