# Spacetime Contact Atlas: A Common Candidate Boundary and Exact Topology-Aware Audit for Deforming Tetrahedral Meshes

**Research manuscript — 25 November 2026**  
**Status:** Complete technical draft for discussion; not a submission-ready claim of full solver or GPU validation. The author list and affiliations must be confirmed before submission.

## Abstract

Collision candidate generation, continuous geometric verification, and physical integration have different correctness contracts. Treating them as interchangeable obscures both safety and performance comparisons. We present a modular collision infrastructure for deforming tetrahedral complexes that makes these contracts explicit. The infrastructure exposes canonical vertex–face and edge–edge candidates with conservative swept feature bounds, retains an exhaustive reference, and allows backend replacement under a candidate-identity guard. A separate exact volumetric audit verifies positive cell orientation and pairwise intersection confined to shared topological simplices throughout a linear trajectory. Moving supporting planes are certified by exact Bernstein coefficients of cubic determinant polynomials. Reusing their quadratic normal coefficients and specializing constant normals preserves the reference decisions while reducing arithmetic cost. On 480 held-out prescribed pair trajectories, all three arithmetic modes produced identical receipts; 4,319 nondegenerate static exact checks at dyadic sample times found no contradiction. In an eight-case held-out volume campaign, the specialized mode achieved 3.088× and 3.101× geometric-mean audit acceleration on GCC and Clang. A separate native hyperelastic floor-contact study produced six physically integrated trajectories per compiler and 7,205 nodal impacts. Matched CPU surface-candidate replay achieved 1.653× and 1.513× geometric-mean exhaustive-to-sweep timing ratios. Independent NumPy/SciPy checks reproduced the native force and update model and all 27,384 candidate identities per compiler. These are evidence for a reference audit and modular candidate contract, not evidence of general superiority, full coupled mesh-contact dynamics, or GPU acceleration.

**Keywords:** continuous collision detection, broad phase, tetrahedral finite elements, exact arithmetic, topology-aware certification, deformable solids, reproducibility.

## 1. Introduction

A broad phase should reject only pairs for which the supplied spatial bounds establish separation. A narrow phase answers a different question: whether those surviving primitive trajectories intersect or approach within a required distance. A physical integrator must additionally account for forces, constraints, contact response and advancement of time. A fast candidate algorithm does not establish a fast solver, and agreement between two candidate algorithms is not proof that a deformable volume remains embedded.

The motivating research question is whether one common candidate interface can support multiple deformable-contact algorithms while allowing independent geometric and physical verification. Earlier workload-specific experiments with persistent sweep-and-prune and guarded controllers motivated this separation; their results cannot be transferred directly to the present volumetric experiments because the workloads, pair contracts and measured stages differ. This manuscript therefore studies the Spacetime Contact Atlas (SCA) as a common boundary with an exact reference audit rather than a new universal contact law.

The contributions investigated here are (i) a canonical feature-candidate contract with explicit topology and conservative motion envelopes; (ii) a sufficient, topology-aware continuous volumetric certificate for linearly moving tetrahedra; (iii) exact arithmetic reuse with receipt-preserving comparisons; and (iv) a reproducible experimental separation between candidate workload, certificate cost and physical dynamics. These are implementation and mathematical contributions under investigation. The novelty of their combination relative to all prior work is not yet established.

## 2. Related work and positioning

Sweep-and-prune, bounding volume hierarchies and exhaustive pair enumeration are established broad-phase approaches. We use them as interchangeable candidate producers, not as new algorithms. Incremental Potential Contact (IPC) [1] is an established variational implicit method for large-deformation contact, with robust intersection and inversion handling. Our explicit reference solver and optional volume audit do not replace IPC or reproduce its solver guarantees. Codimensional IPC [2] addresses additional contact geometry, including mixed-dimensional models; the present proof concerns full-dimensional tetrahedral cells only.

Bernstein sign classification for exact CCD was developed by Tang et al. [3]. The use of Bernstein polynomials, exact signs and interval subdivision is therefore prior art. TightCCD [4] studies efficient robust continuous queries. Wang et al. [5] provide a large benchmark and inclusion-based CCD method, including difficult and degenerate queries. Their work motivates reporting unresolved queries as a separate category rather than incorrectly interpreting conservative failures as collisions. The exact root-parity method [6] and scalable CCD work of Belgrod et al. [7] are important additional comparisons. The latter provides analytically grounded time-of-impact data and CPU/GPU-oriented collision infrastructure. A credible claim of new general CCD performance would require matched evaluation against these implementations on their public benchmarks; that evaluation has not yet been completed.

Our specific question differs from a time-of-impact query: can a pair of linearly moving tetrahedra be certified to intersect only in their *prescribed common topological simplex* during an entire closed interval, and can that certificate be incorporated as an independent audit of a common candidate interface? This is a sufficient safety test, not a complete collision decision or a minimum-distance CCD algorithm.

## 3. Candidate contract and architecture

Let a mesh contain persistent vertex identifiers, edge and face connectivity, category/mask filters and a topology epoch. For every vertex with a cubic Bezier trajectory, the convex hull of its four controls encloses its complete spatial motion. A linear endpoint trajectory is encoded for envelope construction by repeated endpoints `(p,p,q,q)`; this encoding is not passed to a narrow phase as a nonlinear time trajectory. Vertex bounds are enlarged by the specified nonnegative per-vertex padding, with outward binary64 rounding. Edge and face bounds are unions of their incident vertex bounds.

A vertex–face candidate is valid when both masks permit interaction, the vertex is not incident to that face, and their swept boxes overlap inclusively. An edge–edge candidate uses the same mask and overlap conditions, excludes shared endpoints, and orders the two edge identifiers canonically. Each backend returns a sorted unique set. Retained caches are invalidated on changes to the ordered connectivity or topology epoch. The common contract does not authorize arbitrary one-ring filters, narrow-phase culling, or physical response. For external producers with a conservative superset, a binary64 post-filter removes extra pairs; an independent exhaustive guard detects missing pairs before the result is allowed to influence the native solver. This is an audit-only integration and incurs the exhaustive guard cost.

The code separates (A) envelope construction, (B) candidate generation, (C) verification and (D) integration. Candidate timings include A and B when stated, but never imply D unless the full physical path is actually timed. The reference audit uses a distinct swept tetrahedron broad phase; it uses inclusive endpoint AABBs because each of the four source vertex trajectories is linear. Axis selection counts one-dimensional projected interval overlaps. This chooses an inexpensive sweep axis but does not eliminate the worst-case quadratic output size.

## 4. Continuous topology-aware volumetric audit

### 4.1 Model and preconditions

Let K be a finite conforming tetrahedral complex with a consistent vertex identity space. Each tetrahedron has four distinct vertices and a positively oriented, nondegenerate reference embedding. Each stored finite binary64 coordinate at the start and end of a trial is interpreted as its exact dyadic rational value. Over normalized time t in [0,1], the node trajectory is the linear interpolant of those values. Distinct bodies use disjoint global identifier spaces. The proof concerns the declared linear motion, not an unspecified nonlinear physical path between sampled states.

Let A(t) and B(t) be the closed convex tetrahedra of a pair, and S their shared identifier set. The common simplex is the convex hull of the positions of the vertices in S, with the empty set interpreted as no intersection. The goal is to certify that A(t) intersect B(t) equals this prescribed simplex for every t. We also require positive orientation of each cell over the same interval. An inability to find a witness, a degenerate source, or an exhausted subdivision limit does not authorize a safe trial.

### 4.2 Exact arithmetic and polynomial signs

Convert every coordinate to an integer on a common dyadic lattice; multiplying all coordinates by the same positive power of two preserves orientations and separation signs. For a linear vector e(t)=(1-t)e0+t e1 and another f(t), the moving normal n(t)=e(t) cross f(t) is quadratic. For a relative linear vector r(t), define p(t)=det(e(t),f(t),r(t))=n(t) dot r(t). Its cubic Bernstein coefficients, scaled by three to avoid division, are

    C0 = 3 det(e0,f0,r0)
    C1 = det(e1,f0,r0) + det(e0,f1,r0) + det(e0,f0,r1)
    C2 = det(e0,f1,r1) + det(e1,f0,r1) + det(e1,f1,r0)
    C3 = 3 det(e1,f1,r1).

Every Bernstein basis function is nonnegative on the closed interval, and their sum is one. Strictly positive coefficients prove strict positivity of p throughout; weakly nonnegative coefficients prove weak nonnegativity. All zero coefficients prove identically zero. The corresponding negative statements follow by sign reversal. This is sufficient and may be conservative. Dyadic subdivision creates two exact half-intervals; their endpoint lattice is refined by a factor of two, so no floating-point midpoint approximation is introduced. The parent is certified only if both halves are certified.

### 4.3 Pair theorem

**Theorem 1 (sufficient moving-plane certificate).** Consider two convex tetrahedra with linear node motion on a closed interval. For disjoint identifiers, suppose all sixteen signed relative vertex projections along a common moving normal have the same strict sign throughout. Then the tetrahedra are disjoint throughout. For shared identifiers, suppose a moving supporting plane passes through every shared vertex, all vertices of A lie weakly on one side, all vertices of B lie weakly on the opposite side, and every nonshared vertex of at least one tetrahedron lies strictly off the plane. Then their intersection is exactly the shared simplex throughout.

**Proof.** Fix an arbitrary time in the interval. In the nonincident case, the relative projection of any two convex combinations is the corresponding convex combination of the sixteen relative vertex projections. A strict common sign precludes equal points. In the incident case, any point in both convex tetrahedra lies on the supporting plane, since the two tetrahedra lie in opposite closed halfspaces. On the side where all nonshared vertices are strict, a convex combination on the plane can assign positive weight only to shared vertices. Thus the intersection is contained in the shared simplex. Conversely, both tetrahedra contain the convex hull of their shared vertices, establishing equality. The proof holds at every time because the signed conditions hold on the entire closed interval. QED.

We propose coordinate axes, face normals, edge cross products, and incident-simplex-specific directions. For shared vertices, a midpoint dual-cone proposal sums feasible oriented cross products of rays. Candidate normals are only proposals; the exact coefficient tests authorize certification. There is no claim that this search enumerates every possible valid supporting-plane witness.

### 4.4 Whole-complex theorem

**Theorem 2 (sufficient global embedding audit).** Assume K is a conforming abstract complex, its reference embedding is valid, and every cell's oriented volume stays strictly positive on the declared interval. If every distinct pair of tetrahedra intersects only in its common topological simplex at every time, the piecewise-affine map of K is injective at every time on that interval.

**Proof.** At an arbitrary fixed time, the affine map of each tetrahedron is injective by strict positive determinant. If images of two reference points from distinct cells coincide, their common image belongs to the intersection of the cell images. By hypothesis it belongs to the image of the common simplex. Restriction to that simplex agrees between the two affine maps because shared vertex coordinates agree. Injectivity inside each cell forces both reference points to equal the same point of the common simplex. Thus distinct reference points cannot have equal images. This argument applies at every time. QED.

Strict swept AABB disjointness is one valid proof of empty intersection; every remaining pair is tested with Theorem 1. Cell orientation is separately checked using the determinant of three linear edge trajectories, again a cubic. This theorem is about full-dimensional, linearly interpolated complexes, not about continuous trajectories of arbitrary integrators or about physically correct contact forces.

### 4.5 Independent static falsifier

At an exact dyadic sample time, each nondegenerate tetrahedron is the intersection of four rational halfspaces. Intersect the eight halfspaces by enumerating the 56 triples of planes. For each nonsingular triple, homogeneous Cramer's rule gives an exact rational candidate vertex, which is checked against all inequalities. Any nonempty bounded intersection polytope has an extreme point, including lower-dimensional intersections represented by opposing inequalities. If a feasible vertex is outside the shared simplex, the continuous certificate is contradicted. Membership in the shared simplex is checked through the zero barycentric coordinates of nonshared vertices. Static tests can disprove a claimed interval certificate; finite sampling alone cannot prove absence of an event between samples.

## 5. Exact arithmetic optimization

The original determinant expansion repeats cross products for each relative projection. Compute N0=e0 cross f0, N1=e1 cross f0+e0 cross f1 and N2=e1 cross f1 once per proposed axis. The scaled coefficients then become

    [3 N0·r0, N0·r1+N1·r0, N1·r1+N2·r0, 3 N2·r1].

This is algebraic factorization over integers, not a different numerical approximation. When N2=N0 and N1=2N0, the normal is constant even if the input directions move. With a=N0·r0 and b=N0·r1 the coefficients reduce to [3a,2a+b,a+2b,3b]. Constant input directions require only one cross product. Original, factored and specialized modes retain identical witness order, subdivision logic, and receipt counters. The optimized route remains explicitly opt-in in the optional exact audit so older benchmarks remain reproducible.

## 6. Experimental protocol

All reported timing experiments were executed on a shared x86-64 Linux environment using GCC 14.2 and Clang 17, strict C++23 warnings, -O2, no fast-math and disabled floating contraction. The two compilers were run sequentially; they are implementation replications on one hardware environment, not independent machines. We report geometric means of paired block ratios with equal workload weighting. Blocks use balanced ABBA/BAAB or six-slot mirrored rotations after an excluded warmup. Raw repeated blocks characterize repeatability, not a statistical population of independent scenes. No clock isolation or production GPU result is claimed.

### 6.1 Prescribed motion and adversarial controls

The exact static and continuous reference campaigns include dyadic translation controls, touching and intersecting cells, extreme powers of two, shared vertex/edge/face controls, inverted cells, and reconstructed toroidal and U-shaped mesh patterns. The nine whole-mesh prescribed paths in Pass 38H contain 594 cell orientation checks and 26,109 possible cell-pair checks, with 12,546 exact pair certificates and zero unresolved. Adaptive sweep axis selection reduced three-axis overlap tests from 26,109 to 16,245 on those paths; the total exact audit time was approximately unchanged (GCC 0.975×, Clang 1.007×), illustrating that candidate-count savings need not dominate end-to-end cost.

The separate held-out campaign used 480 new trajectories with shuffled identifiers, common shears, independent perturbations, three scales, and all four adjacency strata. All three arithmetic modes produced identical complete receipts. Nine dyadic static samples per path resulted in 4,319 nondegenerate comparisons and one explicitly excluded degenerate sample per compiler. No certified result contradicted the exact static falsifier. Of the 85 unresolved paths, 78 had a sampled forbidden intersection; the other seven remain unclassified, not known safe.

| Shared vertices | Paths | Certified | Unresolved | Sampled forbidden intersection paths |
|---:|---:|---:|---:|---:|
| 0 | 120 | 118 | 2 | 1 |
| 1 | 120 | 94 | 26 | 24 |
| 2 | 120 | 97 | 23 | 21 |
| 3 | 120 | 86 | 34 | 32 |
| Total | 480 | 395 | 85 | 78 |

An independent held-out volume campaign used 24- and 72-tetrahedron structured boxes with static, shear, nonuniform and deliberately inverted paths. Six valid cases certified and both inverted controls were refused by every mode. All receipts matched, covering 9,024 exact pair-path checks across the eight distinct trajectories. The observed acceleration is in the exact audit, not the native force integrator.

| Workload | Compiler | Original/factored | Original/specialized |
|---|---|---:|---:|
| Held-out pairs | GCC | 1.958 | 2.311 |
| Held-out pairs | Clang | 2.039 | 2.327 |
| Held-out complete volume audit | GCC | 2.121 | 3.088 |
| Held-out complete volume audit | Clang | 2.196 | 3.101 |

### 6.2 Native physical source trajectories

The actual native Body::step implementation models 3D compressible neo-Hookean tetrahedral elasticity. For cell rest matrix Dm and current edge matrix Ds, F=Ds Dm^-1 and J=det(F). The strain energy density is

    W(F) = mu/2 (||F||_F^2 - 3) - mu log J + lambda/2 (log J)^2,
    P(F) = mu(F-F^-T) + lambda log(J) F^-T.

The nodal force of a cell is obtained from -V0 P grad_X N_i. Lumped nodal masses are accumulated from reference tetrahedral volumes. Semi-implicit Euler advances velocities and positions. The floor-specific contact algorithm projects a penetrating final nodal position to the plane and applies restitution and tangential friction. It is not the general coupled mesh-contact solver, and its candidate replay is observational rather than part of force integration.

We used three connected toroidal discretizations (48, 120, and 192 tetrahedra), with density 360 kg/m^3, Young's modulus 22 kPa and Poisson ratio 0.32. Free trajectories use zero gravity and a nonuniform initial velocity field. Floor trajectories use 9.81 m/s^2 gravity, an initial downward velocity of 1 m/s, restitution 0.25 and friction 0.2. Damping is zero. Each of the six trajectories has 120 steps of 1 ms. These are small constructed physical cases, not a material diversity or large-scale collision benchmark.

On both compilers all 720 native steps completed; there were 7,205 nodal floor-impact events, a minimum endpoint determinant of 0.890607, and a maximum vertical momentum-ledger residual of 8.211e-13 N s. The residual is Pz_after-Pz_before+M g dt-J_normal. No free case had floor impacts. For the smallest mesh, the exact volume audit certified all 240 linear chords between successive committed states with 84,334 exact pair checks. This proves the specified chord geometry under the certificate's assumptions, not the actual piecewise intra-step floor trajectory. In the larger meshes only endpoint orientation was recorded, not continuous whole-volume certification.

On every tenth step the original endpoint positions and surface connectivity were converted into common feature candidates with per-vertex padding of 0.5 mm. At each of 72 distinct sampled steps we ran five paired ABBA/BAAB blocks, excluding one warmup, with 1,440 timed queries per compiler. Both arms pay feature construction, adjacency filtering and normalization. Canonical candidate sets matched, totaling 27,384 identities across the sampled steps per compiler.

| Tetrahedra | Regime | GCC exhaustive/SAP | Clang exhaustive/SAP |
|---:|---|---:|---:|
| 48 | Free | 1.316 | 1.151 |
| 48 | Floor impact | 1.287 | 1.251 |
| 120 | Free | 1.598 | 1.495 |
| 120 | Floor impact | 1.766 | 1.617 |
| 192 | Free | 1.961 | 1.840 |
| 192 | Floor impact | 2.174 | 1.873 |
| Overall | Equal paired blocks | 1.653 | 1.513 |

These results support CPU candidate replay scaling relative to exhaustive enumeration on the measured small scenes. They do not compare SCA against a tuned production SAP, retained BVH, official IPC solver or GPU system, nor demonstrate an effect on physical runtime.

### 6.3 Independent numerical cross-check

An independent NumPy implementation assembles the same constitutive model with batched linear algebra and scatter accumulation. SciPy cKDTree produces an independent candidate superset using conservative spheres with a measured-scale roundoff cushion, followed by the original box predicate and topology filter. We compare identities rather than counts. A separate high-order SciPy DOP853 solver integrates the free-motion differential equation from the same initial conditions (relative tolerance 1e-10 and absolute tolerance 1e-12); it is not applied to the discontinuous floor response.

| Quantity | Maximum observed absolute discrepancy |
|---|---:|
| Elastic force component | 1.686e-12 N |
| One-step position component | 1.111e-16 m |
| One-step velocity component | 9.194e-16 m/s |
| Normal impulse | 2.665e-15 N s |
| Elastic energy | 6.291e-13 J |
| DOP853 vs native free-motion position component | 9.561e-7 m |
| DOP853 vs native free-motion velocity component | 1.852e-3 m/s |

All 27,384 candidate identities matched the independent spatial query per compiler. The one-step comparison establishes agreement of implementations of the same physical model, not validation against laboratory measurements. DOP853 is a numerical reference with tolerances, not an exact solution. The candidate search cushion is validated for the measured scale only; no general proof of cKDTree conservativeness is claimed.

## 7. Discussion, limitations and remaining acceptance gates

The evidence supports three distinct conclusions: (1) an exact sufficient certificate can check topology-aware intersections under specified linear nodal paths; (2) algebraic reuse accelerates the optional reference audit while retaining identical decisions in the measured cases; (3) native floor-contact source geometry supports reproducible CPU candidate comparisons and independent model checks. The dominant cost depends on the stage: eliminating expensive repeated determinant arithmetic mattered substantially more than reducing inexpensive candidate overlap checks in the exact audit. None of these observations establishes general broad-phase superiority.

Important limitations are the conservative witness search, arbitrary-precision cost, quadratic worst-case candidate output, limited physical scenarios, floor-specific projection, absence of external laboratory validation, and the shared hardware environment. The full coupled native mesh-contact integration, official IPC Toolkit execution, actual Apple Metal hardware, and external full contact dynamics solver validation remain unexecuted in the present environment. The pinned IPC custom AABB API has been inspected and a binary64 superset filter tested independently; this is not upstream execution. The full coupled-leaf visualizer is implemented and schema-tested on synthetic controls, but its real source data are outstanding. The scope of this draft must not be expanded to claim those results until their logs exist. Publication novelty also requires a broader prior-art and dataset review, including direct matched baselines on the Tight Inclusion and scalable CCD datasets.

The next acceptance gates are full coupled physical CPU scaling through widths 1, 4, 16, 32 and 64, independently verified native accepted/rejected leaves, actual Metal candidate and elapsed-time receipts, full external mesh-contact solver integration, and source-linked visualization of actual coupled trajectories. A failed or skipped gate remains open. No submission or merge to main is recommended on the basis of unexecuted tests.

## 8. Reproducibility and artifacts

The source is versioned on the research/spacetime-contact-atlas branch of https://github.com/swayam8624/kyoto-common-contact, with the validation PR #15 remaining draft. Pass 39B closeout records the arithmetic source hashes and four raw held-out CSVs. The physical replay was compiled from ten Git-verified headers at e27353d268cf99845dd361ae2a4723f975702bfd. The replay, independent validation and visualization scripts are in the repository. The local raw evidence archive contains the two complete recorded datasets, source subset, source hashes, figures, and this manuscript. File integrity is listed in its SHA-256 manifest. Measurements can be regenerated with different timing values on another host.

Reproduction of the measured physical experiment (from a checkout with Boost headers):

```sh
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror \
  -ffp-contract=off -fno-fast-math -Iinclude \
  apps/pass39_native_floor_replay.cpp -o /tmp/kyoto-floor-gcc
/tmp/kyoto-floor-gcc evidence/gcc
python3 tools/pass39_independent_replay.py evidence/gcc evidence/independent-gcc.csv
python3 tools/pass39_visualize.py evidence/gcc visuals
```

Repeat with clang++ and a different evidence directory. On a Mac with CMake, Boost, Python scientific packages, ffmpeg and network access to the pinned IPC dependency, the outstanding integrated regression/benchmark suite can be launched with:

```sh
python3 tools/run_final_campaign.py --output /tmp/kyoto-final-$(date +%Y%m%d-%H%M%S) --width 64
```

The runner retains individual failures and marks missing Metal hardware as blocked. The output campaign manifest certifies only whether its listed executable gates passed, not publication novelty or arbitrary solver safety. A separate full external physics integration remains research work even if these regression gates pass.

## References

[1] Li, M., Ferguson, Z., Schneider, T., Langlois, T., Zorin, D., Panozzo, D., Jiang, C., and Kaufman, D. M. 2020. Incremental Potential Contact: Intersection- and Inversion-free, Large-Deformation Dynamics. ACM Transactions on Graphics 39(4), Article 49. https://doi.org/10.1145/3386569.3392425

[2] Li et al. 2021. Codimensional Incremental Potential Contact. ACM Transactions on Graphics. https://ipc-sim.github.io/C-IPC/

[3] Tang, M., Tong, R., Wang, Z., and Manocha, D. 2014. Fast and Exact Continuous Collision Detection with Bernstein Sign Classification. ACM Transactions on Graphics. https://pmc.ncbi.nlm.nih.gov/articles/PMC4283478/

[4] Wang, Z., Tang, M., Tong, R., and Manocha, D. 2015. TightCCD: Efficient and Robust Continuous Collision Detection using Tight Error Bounds. Computer Graphics Forum. https://doi.org/10.1111/cgf.12767

[5] Wang, B., Ferguson, Z., Schneider, T., Jiang, X., Attene, M., and Panozzo, D. 2021. A Large-scale Benchmark and an Inclusion-based Algorithm for Continuous Collision Detection. ACM Transactions on Graphics. https://doi.org/10.1145/3460775

[6] Wang, B., Ferguson, Z., Jiang, X., Attene, M., Panozzo, D., and Schneider, T. 2022. Fast and Exact Root Parity for Continuous Collision Detection. Computer Graphics Forum (Eurographics). https://continuous-collision-detection.github.io/

[7] Belgrod, D., Wang, B., Ferguson, Z., Zhao, X., Attene, M., Panozzo, D., and Schneider, T. Time of Impact Dataset for Continuous Collision Detection and a Scalable Conservative Algorithm. Project and preprint: https://continuous-collision-detection.github.io/ (consulted 25 November 2026).
