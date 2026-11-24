# Pass 25 — Two-child larger-span dual-tree traversal: negative real-FEM result

## Hypothesis from the verified stage profile

The Pass 24 fully accounted real-FEM stage profiler found VF/EE
hierarchy traversal responsible for 73–78% of paid candidate-generation
runtime on Ubuntu and macOS. Improving feature-box bookkeeping alone
would not close the strong cached-input official IPC gap.

The incumbent VF dual-tree implementation expands four child
products whenever both overlapping BVH nodes are internal. An
alternative might split only the larger current bounding region,
generating two child products and reducing recursive calls.

This is an established BVH traversal heuristic, not novel CCD.

## Exact algorithmic change and candidate-safety argument

`RefitHierarchy(..., span_descent=true)` is OPTIONAL and remains
disabled by default. Every source pair of overlapping nonleaf BVH
nodes `(a,b)` splits either

- `(left(a),b)` and `(right(a),b)`, or
- `(a,left(b))` and `(a,right(b))`,

choosing the branch with larger AABB span. Either choice partitions
the complete node-product candidate space without omission.

For off-diagonal edge/edge traversal, the algorithm similarly chooses
which nonleaf region to split, but preserves the exact diagonal
`LL + LR + RR` decomposition and never emits an edge with itself.

The leaf's existing collision categories, masks, incident topology
and conservative swept-AABB comparison are unchanged. All source
boxes, maintainers, normalization and physical inputs remain identical
to the incumbent.

`tests/maintained_broadphase_fuzz_tests.cpp` separately verifies
the new method against the independent exhaustive source oracle
on **1,120 adversarial 3D cubic-motion states**, with masks,
topology changes, high coordinate scales and dense contact cases.

## Native original-physics ABBA methodology

`apps/pass25_traversal_abba.cpp` replays true, physically integrated
compressible neo-Hookean FEM source intervals for 12-, 20- and
24-major toroidal mesh variants, with different materials and drift.
There are 30 sampled physical intervals per scene and eight repeated
independently allocated four-arm benchmarks:

- A1 and A2: new opt-in larger-span VF/EE traversal;
- B1 and B2: incumbent four-child VF/left-first EE traversal.

Every method instance consumes each original source interval
EXACTLY once, and the ABBA method order counterbalances which
implementation runs first. All four candidate sets equal the
independent exhaustive VF/EE oracle.

The paid native wall scope includes topology signature,
source swept-feature update, BVH refit, traversal and candidate
normalization. Source FEM physics and oracle generation are outside
both algorithms' timed scopes.

## Measured results — *do not promote this variant*

[Passing Pass 25 original 3D FEM CI](https://github.com/swayam8624/kyoto-common-contact/actions/runs/45406203196)

Ratio = **incumbent total paid query / optional span-descent total**.
Values below 1 indicate the proposed span-descent algorithm is slower.

| Validated original physical source | Ubuntu | macOS |
| --- | ---: | ---: |
| All warm frames | 0.92375× | 0.90647× |
| 12-major FEM | 0.92169× | 0.9085× |
| 20-major FEM | 0.92481× | 0.9119× |
| 24-major FEM | 0.92408× | 0.9003× |
| A1 / A2 identical-implementation clock ratio | 1.00075× | 0.98842× |
| B1 / B2 identical-implementation clock ratio | 1.00200× | 0.99624× |

On the measured warm states, the new method takes approximately
**8.3% longer on Ubuntu and 10.3% longer on macOS** than the
incumbent. This exceeds the aggregate same-method A/A imbalance
on both platforms. Mac method order was also noisier and must not
be concealed, but the cross-platform direction is consistent.

Eight Ubuntu trials all favored the incumbent; seven of eight
macOS trials favored the incumbent.

## Interpretation and next falsifiable hypothesis

**A smaller number of child pairs generated at one step does NOT
guarantee fewer total visited BVH node products.** The traversal
may repeatedly descend a geometrically poor split, miss
simultaneously tight pruning opportunities from two subtrees,
and spend more work in later recursive calls. Thus the observed
regression is plausible despite each individual step branching
only twice.

This result is a verified negative control; the opt-in alternative
remains disabled by default and has no claimed novelty.

The next meaningful optimization should instrument or reduce the
number of **overlap-tested node pairs and admissible leaf pairs**,
not optimize only the number of child-pair expansions.
Promising independent tests include:

1. An iterative stack with batched child overlap tests.
2. A locally refitted, overlap-aware node-pair traversal policy.
3. Direct measurement of source node-node overlap rejections and
   candidate count versus the exact same original-source IPC LBVH.
4. Strong no-regression requirements on dense and fast-moving FEM.

The published LBVH and other maintained tree implementations remain
the decisive fair baselines. Full solver-step speedup, coupled
deformable/deformable contact and general novelty are not established.
