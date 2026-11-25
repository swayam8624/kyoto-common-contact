# Supervised execution matrix — no hidden claims

Last initialized: 23 November 2026.
The items here describe work to perform, not agents that are already
running. Gate status must be evidenced by native outputs or CI links.

## Geometry and numerical correctness

- [x] Stage-0 exhaustive candidate oracle / fixed SAP parity test source.
- [x] Native CI test on Ubuntu and macOS, Debug and Release.
- [x] Cubic Bezier triangle control-hull enclosure with analytical midpoint contact counterexample and 350 randomized motion cases.
- [ ] Directed-rounding interval arithmetic / independent floating-point proof across all supported FP modes.
- [x] Pass 10 outward-enclosed binary64 sphere support-plane arithmetic with explicit nearest-rounding/IEC559 preconditions and compiler contraction disabled for this verifier.
- [x] Pass 10 exact-dyadic time interval reconstruction from ORIGINAL face endpoints, avoiding possible rounded midpoint gaps.
- [x] Strict conditional IEEE separation of all 288,000 routine + 1,600 fast accepted physical moving-face intervals (0 unresolved) with native plane/dot computation counts archived.
- [ ] Formal FTZ/DAZ/compiler/IEEE predicate proof for every supported FP mode, numerical extremes and arbitrary mesh-mesh CCD; supported-model Pass10 validation is not that universal proof.
- [x] Stage-3 stable VF/EE feature IDs and explicit incident/topology contract, with 140 randomized meshes + 140 reorderings.
- [x] Stage-2 interval-de-Casteljau enclosure (restricted IEEE FP model), independent broad-phase parity and randomized constructive contacts.
- [ ] Independently certify finite-precision handling across all supported FP modes.
- [x] Actual upstream IPC Toolkit linear VF/EE CCD gate authored; full feature-enumeration coverage and analytic tunnelling/near-miss cases (see latest CI for passing status).
- [x] Pass 9 independent exact-arithmetic *triangle/sphere* continuous separation theorem and C++ witness algorithm via six-endpoint swept convex hull (distinct from solver midpoint-Lipschitz certificate).
- [x] Pass 9 independent 7-Voronoi-region finite-triangle distance implementation, including degenerate fallback and actual-FEM second-oracle agreement.
- [x] Reproduce and certify all **288,000** accepted routine moving face intervals and **1,600** fast-impact physical-leaf face intervals: zero unresolved, source-CI artifact preserved.
- [x] Independently audit 1,200 rotated/transformed unequal-mass 3D local barycentric impacts against exact kinetic-work, linear-impulse and angular-impulse identities.
- [x] Pass11 true finite triangle-triangle VF, FV, EE and essential segment/face interior piercing features; check temporal triangle/triangle separation with geometric motion bounds and conservative unknown status.
- [x] Pass11 850 seeded separated 3D moving finite-triangle pairs, analytic interpenetration/coplanar edge/tunnel controls and actual FEM source mesh audit.
- [ ] Fully independent formally rounded *general VF/EE* CCD oracle for moving versus moving triangles and degenerate near-coplanar cases; Pass11 covers static finite rigid triangles but not general IPC.

- [ ] Solver-specific invalidation/version safety audit.
- [ ] ASan/UBSan/fuzz across malformed and extreme cases.

## Baselines, algorithm, instrumentation

- [x] Independently verify and pin official IPC Toolkit stable v1.6.1 source API and release SHA.
- [x] Build and run actual pinned official IPC Toolkit v1.6.1 LBVH with matched vertex/edge/face feature semantics.
- [x] Exact oracle/SAP/IPC parity on 13 native 3D fixtures: 36,799 pairs, zero extras/misses (not performance).
- [ ] Run fair paid IPC LBVH timings on realistic independent solver trajectories.
- [ ] Implement complete dual-tree SCA frontier and safe cache invalidation.
- [x] Measure total paid geometry-to-candidate time in synthetic Stage-2; retain negative depth-zero comparison.
- [ ] Measure real whole-contact-detection/solver time with zero unreported costs.
- [ ] Ablate maintained frontier / local refit / subtree rebuild / fallback.
- [ ] Verify tail cost and static / dense / high-motion negative regimes.

## Pass-6 curved contact and full-face mathematical gate

- [x] Real 900-step tetrahedral neo-Hookean torus vs curved rigid sphere (actual physical motion, analytic swept vertex sphere CCD and impulses).
- [x] Independent static full-surface audit of **all 901 accepted states**: zero vertex penetrations, **458 states with penetrating faces**, minimum face clearance -21.10 mm — genuine negative result.
- [x] Six source-backed figures, 181 actual volumetric ParaView frames, interactive curved impact replay and original native CSVs in verified CI artifact.
- [x] Exact-arithmetic theorem and C++23 implementation for conservative moving-triangle/sphere time-interval separation using Lipschitz midpoint lower bound.
- [x] Controlled interior face tunnelling cases, 640 seeded randomized 3D triangle paths and 128,640 sampled path checks; detect original solver's first offending face transition.
- [ ] Machine-certified directed-rounding implementation for continuous whole-face interval bounds.
- [x] Apply globally synchronized whole-face safe-prefix to actual 3D FEM and distribute effective-mass barycentric normal/tangential contact impulses; zero face overlap on all 901 accepted states.
- [x] Preserve old vertex-only failed 901-state control and run independent 900-step A/B physical-energy/trajectory comparison: 458 versus 0 penetrating states.
- [x] Adversarial extreme 900 m/s whole-face continuous-tunnelling regression: global accepted fraction strictly below one with no accepted face overlap.
- [x] Pass 8 finite-test **actual elapsed-time adaptive replay**: reject full-face-clipped or tet-inverting trial, halve real dt, recompute FEM forces/impulses, accept only fully certified substeps, and verify exact consumed physical duration.
- [x] Cross-platform Debug/Release tests: 40-step free-flight equivalence, 900-step actual FEM surface-safe impact, 50 m/s 15-mm-gap adaptive collision, and all-or-nothing failed-step rollback.
- [x] Native original time-partition/impulse/energy/3D physical stress evidence and 256-finer-step SAME-model numerical reference (not an independent physics solver).
- [ ] An exact contact-time coupled solver that reintegrates event-relative remaining time with global inequality constraints and systematic independent accuracy/convergence bounds.
- [ ] Support the physically violent 900 m/s inversion/budget failure rather than pretending to solve it.
- [x] Verify no static triangle/sphere overlap on ALL 901 accepted whole-face FEM states, with positive tetrahedral Jacobians and finite energy.
- [x] Exact-arithmetic six-endpoint separating-plane cross-check on **every** accepted moving triangle of two source-generated FEM scenarios; max static distance-oracle residual 2.22e-16 m.
- [x] Exact local instantaneous face-contact momentum/energy identities numerically tested across 1,200 rotated 3D impulse fixtures; source receipts record residuals.
- [x] Pass10 source-measured whole-body neo-Hookean FEM gravity+damping+external sphere vector impulse ledger, across all 900 routine and five fast accepted physical leaves.
- [x] Verify internal elastic force and torque cancellation, and transport-corrected full angular momentum balance, with source 3D residuals below 1.5e-12 relative.
- [x] Record mechanical energy evolution without suppressing strong losses: −459.624 J routine and −14545.237 J fast reference, not physically calibrated.
- [ ] Formal machine-certified directed rounding, general feature-level CCD, independent physical-energy/accuracy calibration and pathological boundary robustness.
- [x] Pass11 REAL neo-Hookean FEM response against static connected, OPEN, nonplanar 72-face finite rigid triangle-mesh obstacle: actual mass-weighted contact impulses and continuous whole-face safety.
- [x] Source-verified 900-step physical finite-triangle reference: 517 steps with contact, 11,731 original 3D face impulse updates, zero intersecting accepted rigid/deformable faces, min J 0.72287.
- [x] Preserve old sphere-only blind-to-mesh NEGATIVE control: 95 of 181 sampled real source states intersect SAME rigid 72-face triangle obstacle.
- [x] Pass11C fast 20 m/s real triangular collider impact: 2 unsafe geometric trials rejected, 3 COMPLETE accepted actual physical substeps, 0.750 ms genuinely integrated, min full-face clearance 0.753 mm.
- [ ] General moving CLOSED triangle-mesh rigid/deformable response, robust formally rounded VF/EE feature CCD, self-contact and implicit physically coupled IPC/barrier contact solver.

## Physics scenes and adapters

- [x] First actual freely deforming 3D volumetric hyperelastic solid against flat plane (not yet a self-contact-rich or IPC-constrained solver).
- [x] Verified original rigid nonplanar 72-triangle physical contact FEM trajectory plus material stress, contact impulses, 901 mesh-safe states and all original rig topology.
- [x] Actual adaptive physical time subdivision against the rigid finite triangle collider, with original accepted leaf receipts and no phantom clock advance.
- [ ] Validated self-contact-rich free-deforming 3D solver.
- [ ] Rigid-deformable articulated gripper and shaped object.
- [ ] Net / ring / packaging / tube complex scenes.
- [ ] Independent solver second integration and a named external dataset.
- [x] Real 3D solver-generated tetrahedral hyperelastic torus, gravity and exact flat-plane unilateral contact impulse law: 900 steps, cross-platform mathematical invariants.
- [x] Verified no floor penetration and positive tet J on all 900 accepted steps.
- [ ] Independent general mesh-to-mesh physical contact/penetration audit on the native trajectories (official IPC sampling authored; see latest CI evidence).
- [ ] Optional granular/particle adapter with clear separate evidence.

## Scientific visualization and film

- [x] Stage-1 typed native CSV trace, candidate audit and JSON manifest (kinematic test only; hashes pending).
- [x] Stage-1 3D trajectory, spacetime, time-position and analytical contour plots; offline interactive 3D.
- [x] Stage-1 121-frame ParaView VTK exporter (CI artifact validation pending).
- [x] Stage-3 stable typed VF/EE IDs, 420-vertex/836-face connected 3D kinematic scene, 61 frames and source CSV/VKT exports.
- [ ] Common solver-grade typed trace/scene schema with original state hashes.
- [x] Stage-2 analytic 3D contact-candidate graph and 3D time-slab field visualization (NOT a physical solver).
- [x] Stage-2 six scientific data figures, stand-alone interactive 3D observatory, 120-row native CSV with CI archive.
- [x] Stage-3 source-backed interactive 3D shell/cage mesh, candidate links, displacement field, timeline and volumetric candidate-density visualization.
- [x] Native-source ParaView time series for Stage-3 geometry (61 frames).
- [x] Interactive *actual solver-scene* stress-field playback with the real floor-impulse markers.
- [ ] Actual IPC-derived self/cross-contact graph synchronized to entire solver time series.
- [x] Kinematic vertex displacement scalar field on connected 3D geometry (not stress or strain).
- [x] True solver-derived Cauchy von Mises stress and energy fields, 181 volumetric ParaView states and 3D FEM stress visualization.
- [x] Actual solver-generated high-fidelity VTK cutaway and animated impact loop, verified in its dedicated Linux rendering CI.
- [x] Pass11 source-backed eight-figure nonplanar rigid triangle/FEM physics atlas, offline interactive true-source 3D physical trajectory and original-frame animated GIF.
- [ ] Full strain tensor contour/isosurface and TRUE general moving mesh/mesh/self-contact impulse visualization with independently calibrated physical solver.
- [ ] Instrumented hierarchy / frontier / invalidation 3D overlay.
- [ ] Native SDL3 + Metal renderer and multi-camera replay recording.
- [x] ParaView VTK scalar-field export and optional source-based VTK studio renderer.
- [ ] Blender or Metal film pipeline from actual solver, not synthetic kinematics.
- [ ] 12+ original figures with accessible color maps and data tables.

## Skeptical research audit

- [ ] Prior-art claim-by-claim comparison to current LBVH and 2026 work.
- [ ] Locked independent scene split and preregistered endpoints.
- [ ] Whole-step paired timing with counterbalanced run order / A/A.
- [ ] Scene-cluster bootstrap confidence and P95 failure reporting.
- [ ] Reproduction on Mac and Linux; archive raw receipts off repo.
- [ ] Only then decide novel publishable result versus engineering result.

The release gate is scientific, not aesthetic: zero independently
observed candidate/CCD misses, physically valid solver trajectories,
fair full-cost baselines, and at least two true solver integrations for
generality. Every negative outcome is kept.


## Pass 27–28: solver-facing integration gates (new source, execution pending)

- [x] Pass 27 linear indexed retained VF/EE boundary authored and registered in CTest.
- [ ] Verify 240 independent endpoint-oracle frames and 35 upstream IPC CCD event/prefix parity scenarios on actual CI.
- [x] Pass 28 optional static finite-face BVH inside the ORIGINAL FEM pre/post impulse loops and actual swept whole-face rejection path. Default flat control is preserved. This is a separate specialization, not the Pass 27 VF/EE producer.
- [ ] Verify 1,000 independent ordered finite-face candidate queries, 450 routine physical macrosteps and actual adaptive fast impact, with identical full state and impulse/geometry receipts.
- [ ] Compare full paid wall time across independently repeated scene clusters, including static tree construction and exhaustive retained static audits. Alternating-order exploratory timing is not a publication result.
- [ ] Implement and independently validate moving/deforming two-body contact impulse kernel and synchronized adaptive physical integration, including self-contact as a separate gate.

For the static obstacle the child AABB is enclosed by the parent AABB.
With the same nonnegative gap and finite input, each inequality in
the existing maybe_near predicate is monotone under enlargement.
Hence a rejected parent excludes all descendant leaf pairs, and a
leaf is accepted exactly when the original flat predicate accepts it.
Sorting by original obstacle row preserves the sequential impulse and
early-exit CCD order. This proof preserves the existing predicate, not
a new universal directed-rounded geometric separation proof.

The default flat solver remains unchanged in pair order and early exit.
The tree is rebuilt per macrostep and shared among actual adaptive
leaves, so no stale obstacle is retained. The independent whole-surface
endpoint audits are deliberately unchanged. This is not external IPC
physics, general moving/deforming contact, nor a measured speedup.
CI: .github/workflows/pass28-native-solver.yml.


## Pass 29A — two finite-mass contact law (execution pending)

- [x] Implement local frictionless normal impulses distributed to two
  distinct deformable triangle states with arbitrary positive nodal masses.
- [x] Add independent linear/angular momentum and kinetic-work checks on
  1,600 seeded unequal-mass cases, restitution endpoints and a real
  closest-triangle witness fixture.
- [ ] Confirm source compilation and numerical audit via Mac/Linux CI.
- [ ] Integrate this kernel with two independent volumetric bodies,
  synchronizing adaptive physical time and rejecting unknown moving
  face/face intervals. No coupled solver result exists yet.

The witness relative velocity is u, the unit normal n points from B to A,
and k is the sum of both triangles' weighted squared inverse nodal masses.
For closing u.n, J = -(1+e)(u.n)/k; for separation J = 0. The normal
impulse is applied with opposite signs and normalized barycentric weights.
Total linear impulse vanishes, and the angular impulse is
(p_A-p_B) cross (J n) = 0. The kinetic change equals
-(1-e^2)(u.n)^2/(2k). These are local instantaneous identities, not global
solver accuracy or energy conservation. Friction is deliberately disabled
until an explicit angular-momentum model is justified. No self-contact
or shared-node stencils are supported by this local two-state kernel.


## Pass 29B–C — two volumetric bodies and native physical evidence

- [x] Implement explicit two-body neo-Hookean FEM integration with the
  finite-mass normal law. Both bodies drift on identical physical
  substep clocks; uncertified cross-body swept triangles or nonpositive
  endpoint tetrahedral Jacobians trigger rejection and reintegration.
- [x] Integrate sorted finite-face tree candidates as an optional backend;
  flat cross-body enumeration remains the reference default. The moving
  second-body hierarchy is rebuilt for each relevant endpoint/swept
  geometry. This is not a maintained VF/EE performance result.
- [x] Author unequal-density tetrahedral head-on routine and fast impact
  fixtures, state parity, positive clearance, impulse accounting,
  isolated-system linear/angular momentum and all-or-nothing rollback.
- [x] Optional true accepted substep endpoints for both bodies; output
  trace is replaced only after a successful macro-step. Audit every
  accepted leaf at 33 time samples across every cross-body face pair.
- [ ] Confirm compilation and all numerical gates using actual CI logs.
- [ ] Repeat on dense toroidal volumes and additional geometries,
  independent IPC CCD and external solver integrations.
- [ ] Prove continuous element orientation, support general self-contact
  and containment detection; no universal solver correctness claim.

Initial disjoint volumes, valid self geometry, fixed topology and
frictionless contact are preconditions. The guarded moving/moving
certificate is reused, not a new formally rounded CCD proof. Positive
endpoint Jacobians do not prove absence of in-step element inversion.
Dense samples are regression checks, not independent CCD mathematics.
The kinetic contact work ledger is local; full discrete mechanical
energy conservation is not asserted. Validation is pending.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_coupled_fem_contact_tests --parallel 2
    ctest --test-dir build -R synchronized_two_body_FEM_contact_and_rollback --output-on-failure -V

CI: .github/workflows/pass28-native-solver.yml.


## Pass 30A–B — continuous tetrahedral orientation and discrete energy ledger

- [x] Implement outward interval cubic Bernstein determinant bound for
  every linearly swept tetrahedron in both bodies. The three-times-scaled
  Bernstein coefficients come from mixed endpoint column determinants.
  Strict positivity above the threshold for all coefficients establishes
  a continuous lower bound; dyadic subdivision resolves loose bounds.
- [x] Reject or reintegrate any physical trial whose element orientation
  is unresolved, including positive endpoint but negative interior cases.
- [x] Author 800 randomized affine paths, analytic double inversion,
  interior singularity, and a safely positive path requiring subdivision.
- [x] Require positive continuous lower Jacobian bounds in real coupled
  solver state parity tests.
- [x] Audit actual accepted leaves with independently reconstructed
  force/damping kick and momentum, angular momentum and kinetic contact
  work balances; include a gravity/damping free-flight control.
- [ ] Confirm test compilation and numerical receipts on both platforms.
- [ ] Independent outward continuous *surface* separation and self-contact
  certification remain open; continuous element orientation does not
  establish global injectivity, absence of containment, or surface safety.

FP assumptions: IEC559 binary64, round-to-nearest, no fast-math,
contraction or reassociation, gradual underflow; the source models the
stored inverse-rest matrix as exact binary64 data, not an exact inverse
of an ideal real-valued reference mesh. Other consumers of the header
must compile under the same FP conditions. The discrete energy ledger
accounts for kick and contact work and actual potential changes, not
exact energy conservation, and shares the constitutive force evaluator.
Sampled long-double determinants are regression observations, not an
independent formal oracle. CI results have not been observed.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_tet_orientation_certificate_tests kyoto_coupled_fem_contact_tests --parallel 2
    ctest --test-dir build -R 'outward_continuous_tet_orientation_certificate|synchronized_two_body_FEM_contact_and_rollback' --output-on-failure -V



## Pass 31A–B — synchronized relative hull and connected two-body FEM

- [x] Outward-rounded relative support certificate for two linearly
  moving finite triangles. Every synchronized relative point lies in
  the convex hull of the 18 endpoint vertex differences of a slab.
  Candidate normals (axis, closest witnesses, face normals, edge cross
  products) are verified by interval projection against the clearance
  times the upper normal length; failed candidates cannot certify.
- [x] Make the new certificate the default in the coupled FEM solver;
  retain the guarded Pass 22A comparison arm. Both broad-phase backends
  use the same expanded endpoint-box cull. Unknown triggers rejection.
- [x] Author analytical co-motion, strict skin, crossing, subdivision
  and 300 seeded moving-triangle regression cases.
- [x] Add connected two-volume toroidal fixture (64 nodes, 96 tets,
  128 surface triangles in total), 12 physical macrosteps, equal-state
  flat/tree comparison, positive continuous Jacobian, full endpoint
  gap audit, and mutual finite-mass momentum ledger.
- [x] Native VTK exports for all 13 macro states including volumetric
  cells, source velocity, von Mises stress, nodal normal impulse and
  body ID; native scalar CSV; independent Mac/Linux artifact jobs.
- [ ] Confirm actual execution and inspect numerical receipts and VTK
  outputs. No measured result is claimed until the workflow passes.
- [ ] Add self-contact, independent external IPC surface event audit,
  general containment checks and stronger scale/conditioning tests.
- [ ] Establish paid performance and compare prior art; the relative
  hull is a sufficient separation test, not automatically a novelty
  claim or exact event-time CCD.

The outward proof is conditional on the same IEEE environment as
Pass 30; the support construction models stored binary64 input
coordinates and synchronized linear vertex paths. The root broad-phase
uses finite endpoint extrema and a rounded-to-nearest addition with
a widened nonnegative gap. For representable coordinates, monotonic
rounding cannot make a true coordinate <= exact sum compare greater
than the rounded sum. Overflow may cause an error or conservative
non-pruning, never permission to accept an invalid physical state.
The interval certificate is separate from the approximate closest
feature witness generator. Dense samples share the earlier distance
kernel, so they are not independent exact CCD mathematics.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_rounded_relative_triangle_tests kyoto_coupled_torus_physics_tests --parallel 2
    ctest --test-dir build -R 'outward_relative_moving_triangle_surface_certificate|coupled_toroidal_volumes_physical_parity' --output-on-failure -V
    ./build/kyoto_coupled_torus_physics_tests results/pass31b



## Pass 32A — restricted same-body nonincident surface contact

- [x] Canonical unordered nonincident face/face pairs, sorted flat/tree
  backends, shared-node exclusion and rounded relative swept safety.
- [x] Optional same-body contact impulses integrated into both physical
  kick/drift stages of the coupled FEM solver. Only accepted leaf
  impulses enter the internal work ledger; rejected trials contribute
  computational counters but never mutate the public bodies.
- [x] Separate cross-body and same-body impulse and work accounting.
  A same-body impulse contributes its magnitude to each side's nodal
  receipt (twice the scalar contact impulse when summed across the body).
- [x] Author same-storage two-disconnected-tetrahedron fast collision
  fixture, no cross-body interaction, flat/tree state equality, total
  momentum and contact kinetic work, 33-sample accepted leaf audit,
  incident tetrahedron control and zero-budget atomic rollback.
- [ ] Verify compilation and native execution on Linux/macOS.
- [ ] Connected folded-solid self-contact, adjacent-face nonincident
  VF/EE checks, shared-node generalized Jacobians, and global
  injectivity/containment are NOT solved by the restricted face contract.

The switch is opt-in. Any face pair sharing a vertex is excluded
because ordinary adjacent surfaces have zero distance. This prevents
a shared node from receiving two conflicting overwrite operations, but
can hide collisions away from the shared vertex. No claim of complete
self-collision detection or response follows. A same-body scalar work
ledger is separate from the existing cross-body ledger.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_nonincident_self_contact_tests --parallel 2
    ctest --test-dir build -R restricted_nonincident_same_body_contact --output-on-failure -V

CI is configured in .github/workflows/pass28-native-solver.yml.


## Pass 32B — typed nonincident VF/EE self-contact audit

- [x] Compact surface-only vertices, derive canonical edges, and pass
  actual endpoints through the retained Pass 27 candidate adapter.
- [x] Certify each candidate point/triangle or segment/segment stencil
  through the Pass 31A relative convex-hull certificate, representing
  points and segments by repeated triangle vertices. This does not
  change their convex sets. Failed witnesses return unresolved.
- [x] Construct adjacent shared-edge faces with a nonincident vertex
  crossing the opposite triangle interior. The restricted Pass 32A
  face-pair filter is blind by design; the new typed audit must reject
  the swept path while accepting each separated endpoint.
- [x] Register co-motion, compact mapping and topology-change tests,
  strict FP compilation and Linux/macOS CI.
- [ ] Verify native execution; validate arbitrary degenerate witness
  handling and independent official IPC event parity.
- [ ] Integrate typed VF/EE impulses and audit into actual connected
  folded-solid FEM response. The auditor is currently standalone,
  not a complete self-contact solver or exact event-time CCD.

The adapter applies per-vertex skin padding to both stencils, which
conservatively overgenerates candidates for the requested pair skin.
Only nonincident typed stencils are certified. Initial valid geometry,
complete topology and the explicit IEEE model remain preconditions.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_typed_self_contact_audit_tests --parallel 2
    ctest --test-dir build -R typed_adjacent_face_self_contact_audit --output-on-failure -V


## Pass 32C / 33A — typed response and connected U-solid

- [x] Share original-node typed stencil mapping between continuous
  VF/EE safety audit and physical impulse response. Retain an explicit
  exhaustive candidate reference in the indexed adapter.
- [x] Coalesce signed barycentric weights by original node ID before
  forming effective inverse mass, relative velocity, contact normal,
  or writing velocities. Shared coefficients cancel; repeated IDs
  are squared only after coalescing.
- [x] Opt-in typed self-contact response in both physical stages and
  the actual adaptive safety gate. Separate internal impulse/work
  accounting; disallow simultaneous legacy face-pair self-response.
- [x] Retain candidate hierarchy within a macrostep, with independent
  topology/geometry validation on each query. Cross-macrostep
  persistent ownership is still a separate performance task.
- [x] Author a connected 30-tet U-shaped solid with shared bottom
  bridge and opposing arms; derive boundary triangles from tet-face
  incidence and verify connectivity. Compare eight actual physical
  macrosteps under exhaustive and retained typed candidate backends.
- [x] Author repeated/shared-node analytical mass, momentum,
  restitution/energy identity tests, positive continuous Jacobian
  checks and exhaustive same-method audit of every accepted leaf.
- [ ] Execute on Linux/macOS, verify numerical results and archive
  source evidence. CI source is configured, but runs unconfirmed.
- [ ] Independent official IPC audit, robust initial surface and
  volume validity, manifold accuracy, broader connected folds,
  self-contact performance and actual external solver integration.

The signed coefficient is c_i = alpha_i - beta_i after accumulating
all weights associated with node i. Then k = sum(c_i^2/m_i),
u = sum(c_i*v_i), and each node receives c_i*J*n. Linear momentum
and angular momentum identities follow from sum(c_i)=0 and
sum(c_i*x_i) parallel to n. Gross scalar nodal receipts still
count both sides (including repeated weights), rather than claiming
a net vector impulse magnitude. The proof is local and frictionless.

The connected U fixture is a small constructed volumetric scene,
not yet a dense folded cloth or a calibrated physical benchmark.
The typed audit uses the same rounded relative-hull certificate as
the solver, so the exhaustive check is independent of the candidate
hierarchy, not an independent exact CCD oracle.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_typed_connected_self_contact_tests --parallel 2
    ctest --test-dir build -R typed_connected_FEM_self_contact --no-tests=error --output-on-failure -V

