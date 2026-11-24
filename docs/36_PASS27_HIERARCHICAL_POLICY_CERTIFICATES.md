# Pass 27 — Hierarchical collision-policy relation certificates

Status: experimental branch; do not promote without integrated native CI and full-paid performance comparison.

## Contract and proof

A feature a has category C_a and mask M_a, each a set of bits.
The existing bilateral policy permits a,b iff C_a intersects M_b
AND C_b intersects M_a. For a node N let R_N be the union over
its leaves a of the Boolean relation C_a x M_a.

A permitted leaf pair exists iff R_A intersects transpose(R_B):
the forward direction picks one witness bit from each intersection;
the reverse direction uses the leaves that contributed those two
matching relation entries. This is exact for independent cross products
with all relevant categories represented. For an edge self-product,
the relation can admit a diagonal self witness, so a positive answer
remains only a necessary test for a distinct admissible pair.

For two children, union of both relations equals the parent relation.
Consequently every rejection eliminates a complete Cartesian product.
There is no inference about CCD, incident geometry, actual contact, or
solver response; those checks remain unchanged.

Eight low category bits give a 64-bit packed relation and transpose.
When either node has a high category bit, only the necessary bitwise
union test is used. High mask bits can be ignored in the relation if
both nodes have exclusively low categories. This fallback never truncates
a permitted interaction. Empty category/mask sets are handled naturally.

## Integrated implementation

Opt-in ninth RefitHierarchy constructor argument enables the experiment.
Policy summaries are held separately (32 bytes per BVH node, before vector
overhead), preserving original node layout and ordinary traversal.
Summaries build only at topology invalidation; the complete topology
signature includes feature categories and masks, even when a caller
changes a policy without changing the epoch. Geometry-only refits preserve
the policy summaries. The independent candidate oracle still verifies
full exact VF/EE candidate sets. The counter counts spatially overlapping
node products rejected by policy, not the number of omitted leaf pairs.

## Measured policy-only evidence (local C++20 reference copy)

250,000 node products, 4 leaves per side, fixed seed 812029, 50,000
products per regime. Independent exhaustive leaf-pair predicate. These
are structural rejection counts, NOT physical FEM or query timing.

| Regime | Union rejects | Relation rejects | Unsafe rejects |
|---|---:|---:|---:|
| One-bit mask | 24452 | 44737 | 0 |
| Two-draw mask | 10446 | 34274 | 0 |
| Random 8-bit mask | 2272 | 11355 | 0 |
| Dense multi-bit | 0 | 0 | 0 |
| Wide category | 1960 | 1967 | 0 |

The 200,000 low-category products matched the policy existential oracle
exactly. The 50,000 wide-category products had no false rejection but
retained some impossible interactions. Total extra rejections: 53,203.
The random study does not establish real-scene selectivity or speed.

## Reproduction

```sh
c++ -std=c++20 -O2 -Iinclude apps/pass27_policy_product_study.cpp -o /tmp/pass27-study
mkdir -p results/pass27
/tmp/pass27-study > results/pass27/policy-products.csv
python3 scripts/report_pass27_policy_products.py results/pass27/policy-products.csv results/pass27/policy-products.png
cmake -S . -B build-pass27 -DCMAKE_BUILD_TYPE=Release
cmake --build build-pass27 --parallel 4 --target kyoto_maintained_broadphase_fuzz_tests
ctest --test-dir build-pass27 --output-on-failure -R independent_seeded_3D_feature_broadphase_fuzz
```

## Next evidence gate

Native integrated C++ geometry tests on Ubuntu and macOS, then a
predeclared ABBA of baseline / union-only / relation-aware policies with
identical source geometry and masks, including all-compatible negatives,
selective multi-body interaction graphs, masks changing without epoch,
and separately reported cold/warm cost and memory overhead. Compare
strong IPC only where the baseline policy semantics match. Do not claim
novelty without prior-art review; policy filtering by hierarchy is not
itself a new concept. No general collision or solver speedup established.
