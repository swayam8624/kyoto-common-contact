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



## Pass 34A–B — external CCD audit of accepted and rejected native FEM paths

- [x] Extract shared connected U-solid fixture; native parity and
  external audit use identical initial geometry and material.
- [x] Factor actual native pre-certification force/impulse/drift path
  into a reusable proposal routine, without changing the committed
  physical state or accepted-leaf transaction.
- [x] Independently enumerate all admissible surface VF/EE stencils
  (self and cross-body), run pinned official IPC Toolkit default CCD
  on the original linear source endpoints, and compare detected
  events against both retained SCA and official IPC LBVH.
- [x] Require no IPC CCD events on accepted connected and adaptive
  cross-body physical leaves; require nonzero relevant IPC events
  on both native rejected full proposals and zero candidate misses.
- [x] Export native leaf endpoints, actual feature connectivity, per
  leaf event counts, earliest safe prefix and candidate receipts.
- [x] Register strict FP target under pinned IPC dependency and
  dedicated Linux workflow with uploaded audit receipts.
- [ ] Verify native and IPC executions; no measured results yet.
- [ ] Positive-skin upstream CCD with explicitly verified API,
  further dense connected workloads, physical-model calibration,
  external *solver* integration and scalability remain separate.

The upstream CCD here uses its existing default distance convention
(zero-skin contact); the native skin separation remains a distinct
conditional outward certificate. IPC can return conservative safe
prefixes, not exact impact times. This is independent CCD relative to
the native certificate, but uses the same IPC kernel for exhaustive
event enumeration and filtered event coverage. No IPC solver response
or physical accuracy claim follows.

    cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
    cmake --build build-ipc --target kyoto_pass34_connected_ipc_audit --parallel 2
    ctest --test-dir build-ipc -R pinned_IPC_connected_FEM_accepted_and_rejected_CCD --no-tests=error --output-on-failure -V

CI: .github/workflows/pass34-connected-ipc.yml.


## Pass 35A — audited external broadphase inside native FEM

- [x] Add an optional producer callback at the validated dense-index
  candidate boundary. External results must exactly match the
  normalized exhaustive VF/EE set on every query; otherwise throw
  before any physical state is committed.
- [x] Implement pinned official IPC LBVH producer using identical
  outward-enclosed vertex boxes and dense topology; route it through
  native typed self-contact response and continuous safety.
- [x] Author eight macrostep connected U-solid comparison of retained
  SCA versus actual upstream LBVH, including accepted-leaf endpoint
  and time parity, physical impulse/work equality, and deliberately
  corrupted producer rollback with caller trace preservation.
- [x] Add an independent pinned IPC job and enable PR-triggered native,
  external and candidate-gate workflows for isolated validation base.
- [ ] Observe actual CI compilation and numerical receipts.
- [ ] Full external physics *integrator* embedding, benchmark without
  exhaustive gate, persistent cross-macrostep ownership, paid costs,
  external CCD clearance parity and general self-contact remain open.

This is actual external broadphase interoperability in the native
solver, not a second physics solver and not a runtime speedup.
The exhaustive guard is intentionally paid on every external query.
Internal LBVH traversal counters are unavailable, not zero-work.
Draft PR #15 targets validation/sca-pass34-base (Pass 34 snapshot),
not main. It must not be merged before validation.

    cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
    cmake --build build-ipc --target kyoto_official_ipc_native_backend_tests --parallel 2
    ctest --test-dir build-ipc -R pinned_IPC_LBVH_inside_native_typed_FEM --no-tests=error --output-on-failure -V



## Pass 36A–B — persistent physical ownership and paid cold/warm experiment

- [x] Retain original-node topology mapping and candidate hierarchy
  across actual FEM macrosteps with an optional caller-owned context.
- [x] Check exact ordered face connectivity and node count every query;
  invalidate topology epoch and rebuild on changes.
- [x] Keep actual swept vertex inputs fresh; never cache safety or
  contact response decisions. Clear context on failed bound transaction.
- [x] Account for typed safety candidate box tests, incident rejections
  and certificate interval nodes, previously omitted from the ledger.
- [x] Author cold/warm physical parity, stable rebuild counts, face
  reorder invalidation and failed-transaction recovery regressions.
- [x] Author five-block ABBA/BAAB paid whole-macrostep experiment,
  discarding one complete warmup block; four independent trajectories
  per block, eight macrosteps each. Check physical state and receipts
  after every matched step outside timers. Record raw nanoseconds and
  available warm reuse counters.
- [x] Configure independent Linux/macOS validation and measurement jobs
  with compiler, system and source commit receipts.
- [ ] Execute and inspect actual numerical and timing receipts.
- [ ] Extend to representative size/motion distributions and compare
  SAP, external solver integrations and native GPU implementations
  using matched contracts. Do not infer speedup from reuse counters.

Context is single-owner, not thread-safe. It only persists typed
self-contact mapping and hierarchy; cross-body face tree is still
rebuilt. A validation error before binding has not mutated the
context; a failure after binding resets it. External producer remains
audited and is not a performance arm. Timing includes physical state
copying, force evaluation, candidate preparation, certificates,
response and commit, but excludes fixture construction and parity
assertions. This small workload is a first paid comparison, not
scaling evidence. Hosted runner timing is exploratory.

Draft PR #15 Pass 35 head workflow runs were visible, with jobs queued;
no native or external pass result observed. Local clone DNS unavailable.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_persistent_fem_context_tests kyoto_pass36_paid_persistent_fem_abba --parallel 2
    ctest --test-dir build -R persistent_typed_FEM_physical_parity --no-tests=error --output-on-failure -V
    ./build/kyoto_pass36_paid_persistent_fem_abba results/pass36



## Pass 37A–B — connected physical CPU and native Metal candidate scaling

- [x] Parameterize connected U solid by width cells, arm gap and approach
  speed. 30/120/480/960/1920 tetrahedra correspond to widths 1/4/16/32/64.
  This scales physical width at fixed cell dimensions, NOT fixed-domain
  mesh convergence or independent material/workload families.
- [x] Generate source data exclusively from accepted native FEM leaves
  in two regimes (near contact and separated). Require actual self
  impulses in the near-contact fixture; record orientation receipts.
- [x] Compare exhaustive, rebuilt SAP, rebuilt BVH, retained full-refit
  and retained selective/frontier CPU methods on identical snapshots.
  Each must match the complete canonical exhaustive candidate set.
- [x] Pay each backend's validation, envelopes, traversal, filtering,
  maintenance, and output normalization. Separately record shared
  source snapshot assembly and trace-enabled source integration.
  Five rotated-order measurement blocks and a discarded warmup.
- [x] Validate paired trajectory-block timing summary, geometric mean
  SAP/arm ratios and bootstrap repeatability intervals. Do not treat
  correlated leaf times as independent scenes.
- [x] Implement native Metal rectangular VF/EE overlap-mask baseline.
  Outward float conversion, subnormal avoidance, bounded mask buffers,
  paid host-device dispatch/readback, exact double postfilter and
  adjacency, canonical pair output, exhaustive equality.
- [x] Add explicit large-coordinate and tiny-coordinate GPU precision
  controls. Device-unavailable is reported as skip, never as a result.
- [x] Register Linux/macOS CPU campaign and Apple-only Metal target.
- [ ] Obtain actual CI receipts. Previous PR jobs remain queued, local
  repository DNS unavailable. No measured timing or device claim.
- [ ] Optimize GPU candidate compaction/hierarchy, test independent
  external physical integrator, extend workload families and sizes,
  adversarial evidence, run on dedicated hardware.

GPU method is an O(VF + E^2) rectangular mask baseline, not a GPU
implementation of the maintained SCA hierarchy. CPU postfilter and
mask readback are included in measured query time; initial pipeline
compilation is excluded and the first block is discarded. CPU and
Metal campaigns each have matched within-run SAP controls. Do not
directly compare absolute timings across different runners.
Default CI maximum width is 16 (480 tets), with 32 and 64 selectable
locally. The workload is a small constructed connected FEM campaign,
not sufficient evidence of general asymptotic superiority.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target kyoto_pass37_cpu_physical_scaling --parallel 2
    ./build/kyoto_pass37_cpu_physical_scaling results/pass37 64
    python3 tools/summarize_pass37_scaling.py results/pass37/matched_cpu_candidate_scaling.csv results/pass37/paired_summary.json

On a Mac with an actual Metal device:

    cmake --build build --target kyoto_pass37_metal_physical_scaling --parallel 2
    ./build/kyoto_pass37_metal_physical_scaling results/pass37-metal 64

CI: .github/workflows/pass37-cpu-scaling.yml.


## Pass 38A–B — initial cross-volume containment and exact SAT validation

- [x] Add independent convex tetrahedron separating-plane certificate.
  Propose coordinate axes, tetrahedron face normals and edge-cross
  axes; verify all 16 relative vertex projections with outward
  binary64 arithmetic. Strict separation only; unknown fails closed.
- [x] Default initial cross-body volume gate in coupled FEM, including
  body-level disjoint AABB fast path and exhaustive overlapping
  tetrahedron-pair checks. Record paid work counters.
- [x] Author integrated nested-volume counterexample: surfaces
  separated, volumes contained; require rejected physical transaction,
  unchanged state and unchanged caller trace. Test disjoint oblique
  pair and body-level fast path.
- [x] Execute standalone tests on Linux x86_64 GCC 14.2.0 and Clang
  17.0.0 with C++23, -O2, -ffp-contract=off, -fno-fast-math.
  Both compilers: 2,000 known-contained plus 2,000 known-separated
  seeded checks and analytic controls passed.
- [x] Execute independent bounded int64 exact SAT oracle on both
  compilers: 10,000 independently generated nondegenerate integer
  tetrahedron pairs, each under three exactly representable dyadic
  coordinate transforms; 30,000 comparisons, 11,694 exact-separated,
  18,306 exact-nonseparated, 0 false clears, 0 conservative unknowns
  among exact-separated cases. These are correlated transforms,
  not 30,000 independent scenes.
- [x] Verify the three tested local file Git blob hashes exactly
  match the repository blobs:
  kernel c168f54a3bcbcfadda25198177f2d53cc2480416;
  analytic test 6ac53918aee2a014740178c76788ed1bb32b545f;
  exact oracle 521db9746ccdf7f197eb4e1c951493b044dfeaed.
- [ ] Execute integrated FEM containment gate and all accumulated
  solver/IPC/Metal jobs. Earlier CI remains queued; local full clone
  fails DNS. Only standalone kernel results are verified here.
- [ ] Same-body volumetric injectivity, completeness of supplied
  boundary topology, nonincident internal-cell overlap, and general
  robust CCD remain separate unresolved issues.

The separating-plane proof follows from convexity: every point
difference is a convex combination of the 16 vertex differences.
If their outward-enclosed projections all have strictly the same
sign, the two convex solids cannot share a point. The proof is
conditional on the supported IEEE model; approximate axes need not
be exact for a successful witness to be valid. The initial volume
check is not a continuous volumetric certificate by itself.

Local standalone reproduction (run from repository root):

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -Iinclude tests/initial_tet_separation_tests.cpp -o /tmp/kyoto-tet-sat
    /tmp/kyoto-tet-sat
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -Iinclude tests/initial_tet_exact_oracle_tests.cpp -o /tmp/kyoto-tet-oracle
    /tmp/kyoto-tet-oracle

Verified locally on exact source blobs, not inferred from CI status.


## Pass 38C — complete combinatorial collision boundary and CI queue control

- [x] Validate unique tetrahedra, in-range distinct node IDs, all
  nodes used, interior face incidence and opposite orientation,
  boundary edge degree two, and a single cycle in each boundary
  vertex link. Require supplied unordered surface triangles to equal
  the incidence-derived boundary exactly.
- [x] Local GCC/Clang C++23 -O2 -Wall -Wextra -Wpedantic -Werror:
  1,000 valid renumbered complexes, 1,000 missing-face rejections,
  and analytic duplicate, orientation, nonmanifold, disconnected and
  pinched-boundary controls passed. GCC ASan/UBSan also passed.
- [x] Integrate boundary validation before actual coupled FEM trials;
  add missing-boundary transactional regression. Integrated execution
  remains unverified.
- [x] Consolidate five recent workflow triggers to PR/manual only,
  with cancellation of superseded runs within each workflow and
  source branch. Existing queued runs may remain; no CI result inferred.

This is a combinatorial boundary check, not a certificate of internal
vertex/edge links, geometric embedding, global injectivity, or continuous
volumetric noninterpenetration. Supplied triangle winding is ignored for
two-sided contact. Validation is currently paid each macrostep.

Local kernel source Git blob: 8a552f257e199dc83f1079b3873f06ee888a17f4.
Local test source Git blob: 9d1db0e445b080208ebcf5e8e2a4235bd35c666b.

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/volume_boundary_topology_tests.cpp -o /tmp/kyoto-boundary
    /tmp/kyoto-boundary



## Pass 38D — compatibility with scalable volumetric mesh construction

- [x] Independently reconstruct the native toroidal and U-solid
  connectivity patterns without depending on the entire FEM solver.
  Run the production boundary checker on 27 toroidal resolutions
  (major 8/20/64, radial and vertical 2/3/8) and 5 U widths
  (1/4/16/32/64). Verify analytical boundary triangle counts.
- [x] Local GCC and Clang strict-warning C++23 runs both passed
  all 32 meshes, totaling 58,710 tetrahedra. Standalone source blob
  cde1d826e07cc9a54be8cf20d300951c0e5412be.
- [x] Register direct native Body-factory compatibility test using
  identical size combinations and analytical expected counts.
- [ ] Execute direct native factory and solver tests. The local
  standalone result verifies the reproduced connectivity patterns,
  not actual native FEM factory execution or physical trajectories.



## Pass 38E — exact static same-body volume oracle

- [x] Convert finite binary64 source coordinates to a common exact
  dyadic integer lattice. Use Boost.Multiprecision only in an optional
  reference target, not in the production solver.
- [x] Enumerate all 56 triples of tetrahedron halfspace planes;
  homogeneous integer Cramer's rule and all eight inequalities check
  intersection vertices exactly. For incident tetrahedra, all feasible
  vertices must belong to the shared simplex; for nonincident cells,
  there must be no feasible vertex. This is static, not continuous.
- [x] GCC 14.2 and Clang 17 strict-warning C++23 execution, plus GCC
  ASan/UBSan: 2,000 independently generated dyadic translation tests
  (1,170 separated, 830 intersecting), shared simplex positive and
  negative controls, extreme exponents -1070 through +1000, conforming
  12-cell mesh and deliberately folded cell counterexample passed.
  The first GCC build hit a Boost left-shift warning; replaced that
  conversion with explicit bit setting and reran all three builds.
- [x] Add optional native accepted-endpoint audit on connected solid
  and remote torus, requiring actual self-contact impulses.
- [ ] Execute native endpoint integration; static endpoint checks
  alone cannot prove continuous self-volume injectivity. Exact
  all-pairs reference is deliberately not a performance method.
- [ ] Full native/IPC/GPU evidence still requires actual execution.

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/exact_tet_intersection_tests.cpp -o /tmp/kyoto-exact-static
    /tmp/kyoto-exact-static
    cmake -S . -B build-exact -DKYOTO_WITH_EXACT_SELF_VOLUME=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build-exact --target kyoto_exact_tet_intersection_tests kyoto_native_exact_self_volume_tests --parallel 2
    ctest --test-dir build-exact -R 'exact_static_tetrahedral_intersection|exact_static_native_FEM_endpoint_self_volume' --no-tests=error --output-on-failure -V



## Pass 38F — exact static fixture campaign and independent failure receipts

- [x] Run exact static oracle on four independently reconstructed
  mesh patterns on GCC and Clang. Identical receipts on both:
  torus8 (48 cells, 1,128 pairs, 345 exact, 296 incident);
  torus20 (120 cells, 7,140 pairs, 993 exact, 740 incident);
  U1 (30 cells, 435 pairs, 291 exact, 190 incident);
  U4 (120 cells, 7,140 pairs, 3,000 exact, 1,558 incident).
  Total 15,843 pairs, 4,629 exact checks, 2,784 incident checks,
  no invalid pair. These are reconstructed static mesh patterns,
  not native FEM factory calls or accepted physical endpoints.
- [x] Preserve the original 32-configuration topology campaign
  under the default standalone test; exact fixture mode is optional.
- [x] Add Ubuntu exact-reference CI job with explicit Boost dependency.
- [x] Add tools/validate_native_research.py: independent configuration
  groups, per-target build/test logs, anchored CTest selections,
  no-tests-as-error, explicit skips and JSON manifest. Never
  overwrite an earlier evidence directory; return nonzero on failure.
  Local integration test using a small actual CMake project proved
  continued execution after build failure, test failure, missing
  test and skipped test. Python byte-compilation also passed.
- [ ] Run the runner against the full repository. DNS/queued CI
  remain blockers. Native, IPC, and Metal results are not inferred.

From the repository root on a Mac with CMake, Boost headers and Metal:

    python3 tools/validate_native_research.py --output /tmp/kyoto-validation-$(date +%Y%m%d-%H%M%S) --exact --ipc --metal

Omit --exact or --ipc for unavailable optional dependencies, and
--metal on non-Apple hosts. The script runs 17 native gates and
optionally 3 exact, 2 IPC and 1 Metal gates. It does not execute the
separate full scaling campaigns.



## Pass 38G — exact continuous moving separating-plane certificate

- [x] Use exact binary64 dyadic coordinate conversion and cubic
  integer Bernstein coefficients of determinant(e(t),f(t),r(t)).
  A direction cross(e,f) may move in time. For disjoint cells,
  all sixteen relative vertex projections must have strict sign.
  For incident cells, shared vertices lie identically on the
  moving plane, both sides have weak opposite signs, and one
  side has strict signs for every nonshared vertex. The common
  simplex is then exactly the intersection for the entire slab.
- [x] Exact dyadic subdivision; unresolved remains unknown.
  Continuous cell nondegeneracy/orientation is a separate condition.
- [x] Locally execute GCC and Clang strict-warning C++23 builds and
  GCC AddressSanitizer/UndefinedBehaviorSanitizer: 1,000 seeded
  analytically classified translated simplex paths (884 separated,
  116 intersecting), 884 certified, zero false clears, zero unknowns
  among separated paths. Shared-face/edge/vertex, swapped, moving
  plane and extreme scale controls passed.
- [x] First shared-vertex positive control was unresolved; add
  diagonal constant directions and rerun all tests.
- [x] Register optional Boost reference target and validation runner.
- [ ] Evaluate certification coverage on whole moving meshes.
  Incomplete proposed directions can leave valid incident pairs
  unresolved; this is a sound sufficient certificate, not an exact
  complete continuous collision decision procedure.
- [ ] Native/IPC/GPU integrated execution remains unavailable;
  latest Pass 38F workflows are queued.

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/exact_swept_tet_certificate_tests.cpp -o /tmp/kyoto-swept-exact
    /tmp/kyoto-swept-exact

Locally tested source blob: 9ecedf1fb908914a4c48f66639f3eac6fd35a7ca.
Test blob: 6b2aba12d3c8c69bf3108d5fa5e00775e3413e2b.


## Pass 38H — whole moving mesh exact reference

- [x] Exhaustive whole-volume linear path audit: positive exact
  orientation for every cell and strict swept AABB rejection or
  exact moving-plane certificate for every remaining cell pair.
- [x] Classify unresolved pairs by shared vertex count.
- [x] Initial GCC run certified eight of nine prescribed fixture
  paths; larger U shear had two unresolved shared-vertex pairs.
  Add exact midpoint dual-cone witness (sum feasible oriented
  cross products) followed by the unchanged exact slab sign test.
- [x] GCC 14.2 and Clang 17 strict-warning C++23 execution both
  certified all nine paths (torus8, U1, U4; static, shear and
  nonuniform deformation). Identical CSV receipts: 594 cell-path
  orientation checks, 26,109 pair-path checks, 13,563 swept AABB
  rejections, 12,546 exact pair certificates (6,414 nonincident,
  3,360 shared vertex, 1,920 shared edge, 852 shared face).
  Zero unresolved among the nine paths; inverted-cell control
  not certified. Prior 1,000 analytic pair tests also rerun on
  both compilers with unchanged results.
- [x] Register reference campaign in CTest/CI and local runner.
- [ ] Actual native accepted leaf audit and external/GPU execution.
  This campaign uses reconstructed geometry and prescribed paths,
  not forces or measured simulation. Unknown remains possible
  for other geometry and no general completeness claim is made.

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/exact_swept_volume_fixture_tests.cpp -o /tmp/kyoto-swept-volume
    /tmp/kyoto-swept-volume

Tested Git blobs: pair certificate 4066a4db2b95cc16aef3d59dfeb3c1cc1b83c66f;
whole volume c1b7c61aa651ecb55f84399137040cb4f3c7685f;
test 28644870058daa6f37dbec5c0b7c58fb790b0639;
fixture 38fb36a425fc738f9f4ff45e7bdeaec987a412db.


## Pass 38I — matched whole-volume broad phase and native leaf audit

- [x] Add a dependency-free inclusive swept cell SAP with exact
  projected-overlap-count axis selection and canonical pair order.
- [x] GCC/Clang strict-warning C++23 runs: 1,000 seeded box scenes,
  12,360 candidate pairs matched exactly; exhaustive 1,478,460 vs
  sweep 278,544 three-axis overlap tests. Sparse 4,096-cell case
  selected its separating axis and required zero three-axis tests.
- [x] Rerun all nine exact whole-volume prescribed fixture paths on
  both compilers. Exact certificate work, decisions, pair identity
  and unresolved counts equal exhaustive; sweep 16,245 vs exhaustive
  26,109 three-axis overlap tests. No runtime speedup asserted:
  sorting, active scans and output normalization are additional work.
- [x] Refresh and retain matching CSV receipts, including the earlier
  rerun of Pass 38H (old local CSV contained pre-fix unresolved case).
- [x] Register an optional actual native continuous leaf test for
  combined two-body geometry, with disjoint global index namespaces,
  explicit exhaustive parity and unresolved diagnostics.
- [ ] Execute native integration and full external/GPU gates; latest
  PR runs remain pending. Native continuous test is authored only.
  The exact verifier is sufficient, not complete, and no generic
  solver-wide safety or performance claim follows from these tests.

Tested source blobs: candidate header 64da4134463881c5e35f4e790ac090447bcb7e8c;
candidate test dbb0029583bff71f395f91aa92bf2ff06b9aa710;
volume audit d8496416da9f05545aa5163e699a6f3ffb5cb7df;
fixture test 381f42aca909135261e8d85e4df9372466706dd7.



## Pass 39A — measured whole exact-volume audit cost

- [x] Execute five paired ABBA/BAAB blocks per case (two evaluations
  per arm), with one excluded warmup, on all nine Pass 38I prescribed
  fixtures. GCC and Clang runs executed sequentially: 360 timed audits
  total, 180 per compiler. Every run matched exact reference receipts.
- [x] Preserve both unmodified raw timing CSVs under docs/evidence.
  GCC blob 96f611ba33fe0db1bda00edc6794bc1e1648c929;
  Clang blob 13c35320f29dd9fb6808c8fe4de5c3eef3687bd6.
- [x] Strict C++23 -O2 -Wall -Wextra -Wpedantic -Werror
  -ffp-contract=off -fno-fast-math builds, GCC 14.2.0 and Clang 17.0.0;
  Linux x86_64 container, AMD EPYC 9V74, 5 visible CPUs, CPU quota
  4 cores, 4 GiB memory limit. Shared environment, no controlled
  clock or load isolation; descriptive results only.
- [x] Validate raw rows, expected order, reference work and polynomial
  counts within each paired block; reject incomplete block. Provide
  reproducible timing and overlap-work plots and paired-block CSV.
- [x] Descriptive geometric mean of paired exhaustive/sweep elapsed
  time ratios: GCC 0.9752548871x, Clang 1.0071758602x. These small
  whole exact audits do not demonstrate meaningful overall speedup.
  Candidate filtering decreases overlap tests, not exact work.
  Certificate-dominated cost is a plausible explanation, not a
  separately profiled attribution. Min/max bars are observed blocks,
  not confidence intervals or independent workload samples.
- [ ] Actual native FEM trajectories and independent solver/GPU
  validation are still pending. These are prescribed paths, not FEM.

Reproduce the analysis (optional Matplotlib for PNG output):

    python3 tools/summarize_pass39.py docs/evidence/pass38i_swept_volume_gcc_clang.csv docs/evidence/pass39-gcc.csv docs/evidence/pass39-clang.csv results/pass39 --plots

Rebuild the measured benchmark from repository root:

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass39_exact_volume_abba.cpp -o /tmp/kyoto-pass39-gcc
    /tmp/kyoto-pass39-gcc > /tmp/pass39-gcc.csv

Use the same flags with clang++ for the second arm. Source hash
43d5cb6031b8404b2f662d9543a0892ee6dcb277 and analysis script hash
70a9d262040735d18e65776705c75f19e5719f46 match locally executed files.


## Pass 39B — reuse exact separating-axis normal polynomial

- [x] Factor cubic determinant projections through the exact quadratic
  normal coefficients N0=e0×f0, N1=e1×f0+e0×f1, N2=e1×f1.
  The scaled cubic coefficients are 3N0·r0, N0·r1+N1·r0,
  N1·r1+N2·r0, 3N2·r1. Compute the normal once per proposed
  axis and retain the original expansion as an explicit control.
- [x] GCC 14.2 and Clang 17 strict-warning C++23 execution: 10,000
  exact coefficient comparisons (integer shifts 0,64,1074) and
  1,000 full swept pair receipt comparisons on each compiler.
  Fixed a Boost shift diagnostic in the test generator by explicit
  bit setting, rather than weakening warning flags.
- [x] Paid comparison: identical sweep candidate generation and
  exact certificate receipts, 3 ABBA/BAAB blocks per each of nine
  prescribed fixtures plus an excluded warmup. 108 timed whole
  audits per compiler, 216 total. Geometric mean reference/factored:
  GCC 2.1680193836x (block range 1.919999–2.446978);
  Clang 2.1773600349x (1.893871–2.389381).
  Shared Linux x86_64 AMD EPYC 9V74 container, sequential timing.
- [x] Preserve raw timing CSV and exact tested source blobs.
- [x] Register optional coefficient parity test in CMake and CI.
- [ ] Keep factored path opt-in pending further held-out adversarial
  validation. The reference default and previous benchmarks are
  preserved. No native FEM or GPU acceleration claim; prescribed
  trajectories and exact audit only. Full integrated execution pending.

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/exact_factored_axis_tests.cpp -o /tmp/kyoto-factored
    /tmp/kyoto-factored
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass39b_factored_axis_abba.cpp -o /tmp/kyoto-pass39b
    /tmp/kyoto-pass39b > /tmp/pass39b-gcc.csv

Use clang++ with identical flags for the second compiler. Raw CSVs
are docs/evidence/pass39b-gcc.csv and pass39b-clang.csv. Ratio per block
is sum(reference ns)/sum(factored ns); equal-weight geometric mean
over all 27 blocks per compiler. Ranges are observed, not confidence
intervals.


## Pass 39B2 — exact constant-normal specialization

- [x] Extract the tested factored normal polynomial helper. Preserve the
  default determinant route and prior factored route, add an opt-in
  constant-normal mode. Constant directions use one cross product;
  moving directions whose exact normal coefficients satisfy N2=N0 and
  N1=2N0 also qualify. Project using two dot products rather than six:
  [3a,2a+b,a+2b,3b], a=N0·r0, b=N0·r1.
- [x] Compile standalone helper on GCC 14.2 and Clang 17 with strict
  C++23 warnings-as-errors and no fast-math. Each passes 24,000
  coefficient comparisons versus independent eight-determinant
  expansion: 1,000 axes in each of three groups, eight rays per axis,
  integer shifts 0,64,1074. Zero/sign-changing controls passed.
- [x] Execute paid microbenchmark including normal construction,
  projection and exact checksum. Five ABBA/BAAB blocks per group,
  excluded warmup, 60 timed batches per compiler, 8,000 projections
  per batch. Geometric mean prior factored / specialization times:
  static directions GCC 2.71543x Clang 2.86297x;
  moving constant normal GCC 2.27239x Clang 2.41000x;
  general moving normal GCC 1.03465x Clang 1.02982x.
  Shared Linux container; ratios descriptive, not population estimates.
- [x] Preserve exact tested helper and test source hashes, raw
  timing CSVs (pass39c-gcc.csv and pass39c-clang.csv; filenames
  reflect the experimental run, not completion of physical Pass39C).
- [x] Wire optional mode into pair and whole-volume APIs; register test.
- [ ] Compile and execute full integrated held-out pair/mesh tests.
  This run validates the exact helper and measures a synthetic
  projection kernel, not full pair, mesh or FEM performance.
  Full native CI remains queued/pending; no new physical claim.

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/exact_constant_axis_tests.cpp -o /tmp/kyoto-constant-axis
    /tmp/kyoto-constant-axis --timing

Tested source blobs: helper 3cb726b9111438e10b2121be9222bb2e99872e9d;
test 742d36612b5eb58584ed7fc6f17dd4f37f3638f8.

## Pass 39B — CLOSED: exact arithmetic optimization milestone

The previously named B2 is part of this single optimization milestone,
not a separate unfinished pass. The original determinant expansion,
factored normal, and constant-normal specialization are now compared
at coefficient, complete pair, and complete volume-audit level.

### Held-out execution

- [x] Recover six actual repository headers into the local Linux
  execution environment, verifying all six Git blob hashes against
  pinned commit 5714095a00b457b3fd8a36fe48c918c01d79a01a. Compile on GCC 14.2 and Clang 17,
  strict C++23 warnings-as-errors, -O2 -ffp-contract=off -fno-fast-math.
- [x] 480 new dyadic paths (seed 3917129, 120 per adjacency stratum,
  scales 2^-500, 1, 2^500, mixed common shear and independent
  perturbation, shuffled vertex indices). All three full pair
  receipts equal; swapped reference/specialized receipts equal.
- [x] Independent exact static intersection at nine dyadic times
  per trajectory: 4,319 nondegenerate comparisons per compiler,
  one degenerate sample excluded and explicitly counted. Zero
  contradictions of either continuous direction. 395 certified,
  85 unresolved; 78 paths with a sampled forbidden intersection.
  The remaining seven unresolved paths are not claimed collision-free.
  Sampling is a falsifier, not a complete continuous oracle.

| Shared vertices | Paths | Certified | Unknown | Sampled hit paths | Static checks |
|---:|---:|---:|---:|---:|---:|
| 0 | 120 | 118 | 2 | 1 | 1080 |
| 1 | 120 | 94 | 26 | 24 | 1080 |
| 2 | 120 | 97 | 23 | 21 | 1079 |
| 3 | 120 | 86 | 34 | 32 | 1080 |

- [x] Held-out full-volume audit: 24- and 72-tet structured boxes
  with static, shear, nonuniform and deliberately inverted paths.
  All six valid cases certified and both inverted cases refused,
  with identical complete work receipts for all three modes on both
  compilers. 9,024 exact pair-path checks across the eight distinct
  mesh trajectories (not independent physical simulations).
- [x] Paid six-slot mirrored Latin rotations, three measured blocks
  per stratum/case, one excluded warmup. Pair timing: 72 batches per
  compiler, 120 paths per batch; mesh timing: 144 audits per compiler.
  Timing includes candidate generation for the mesh audits, exact
  orientation, witnesses and verification. Equality checking follows
  each timed call. Raw rows and work-counter parity checked.

| Workload | Compiler | Reference/factored | Reference/specialized | Factored/specialized |
|---|---|---:|---:|---:|
| Held-out pairs | GCC | 1.958110 | 2.310519 | 1.179974 |
| Held-out pairs | Clang | 2.039014 | 2.327196 | 1.141334 |
| Held-out meshes | GCC | 2.121471 | 3.088246 | 1.455710 |
| Held-out meshes | Clang | 2.196383 | 3.100765 | 1.411760 |

Equal-weight geometric means of paired block sums; repeated blocks
measure repeatability, not independent scene population inference.
Shared Linux VM, no isolated hardware or production FEM timing.

### Decision and scope

- [x] Freeze the arithmetic work. Recommend explicit
  `factored=true, constant_fast=true` for the optional exact audit;
  preserve historical default and explicit original arm for
  reproducibility. No approximation or threshold change.
- [x] Register complete pair/volume parity regressions and preserve
  four raw CSV files with exact locally executed blob hashes.
- [x] Pass 39B acceptance complete at standalone exact-audit scope.
  Do not extend this milestone with additional arithmetic subpasses.
- [ ] Pass 39C: integrated native physical CPU/GPU comparisons.
  Full clone still fails DNS; queued/pending CI has not established
  native solver build or execution. External solver integration and
  manuscript remain open. No whole-FEM speedup is claimed.

Reproduce on a checkout:

    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/exact_axis_heldout_tests.cpp -o /tmp/kyoto-heldout
    /tmp/kyoto-heldout --timing > /tmp/heldout-gcc.csv
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/exact_axis_heldout_volume_tests.cpp -o /tmp/kyoto-heldout-volume
    /tmp/kyoto-heldout-volume > /tmp/mesh-gcc.csv

Use clang++ with the same flags for the other compiler. Recorded CSVs:
`docs/evidence/pass39b-final-{heldout,mesh}-{gcc,clang}.csv`.

## Passes 39C–39E — executed physical floor-contact evidence, independent reference and visualization

- [x] Recover and hash-verify ten actual headers at pinned source
  e27353d268cf99845dd361ae2a4723f975702bfd; use native Body::step
  with neo-Hookean elasticity and analytic floor projection.
- [x] Six 120-step physical trajectories per compiler (48/120/192
  tetrahedra, free and floor-impact); 7,205 nodal impacts per compiler,
  minimum endpoint J 0.89060718118, maximum absolute vertical momentum
  residual 8.21121e-13 N s.
- [x] Surface candidate replay: 72 distinct sampled steps, five
  ABBA/BAAB blocks each, excluded warmup, 1,440 timed queries per
  compiler. All 27,384 canonical candidate identities match exhaustive
  reference. Geomean exhaustive/SAP 1.652725 (GCC), 1.512898 (Clang).
  Candidate replay does not change the analytic floor integrator.
- [x] All 240 linear chords between committed states for 48-cell
  geometry independently certified with exact volume audit
  (84,334 candidate cell pairs, no unknown). Chord certification
  is not the actual intra-step piecewise floor-impact trajectory.
- [x] Independent NumPy/SciPy assembly and one-step replay for all
  720 steps; cKDTree candidate parity and free-motion DOP853
  comparison. Both compilers' independent CSV identical.
  Largest component force error 1.686e-12 N, position 1.111e-16 m,
  velocity 9.194e-16 m/s, impulse 2.665e-15 N s.
- [x] Generate fixed-camera actual recorded 3D stress animation,
  orientation and contact impulse figures and paired timing plot.
  Reproducible visualization code and evidence summaries committed.
  Raw local data archive retained separately; SHA-256 manifest names
  each raw file. Do not mistake the manifest for uploaded raw bytes.
- [ ] Pass 39C full coupled solver CPU and real Metal hardware.
- [ ] Pass 39D upstream IPC native backend and independent mesh-contact
  dynamics integration. SciPy is numerical validation only.
- [ ] Pass 39E coupled mesh-mesh accepted/rejected trajectory visualizations.

Reproduction with C++23, Boost, NumPy, SciPy, pandas, Matplotlib and ffmpeg:
    cmake -S . -B build-floor -DCMAKE_BUILD_TYPE=Release -DKYOTO_WITH_EXACT_SELF_VOLUME=ON
    cmake --build build-floor --target kyoto_pass39_native_floor_replay --parallel 2
    ./build-floor/kyoto_pass39_native_floor_replay evidence/gcc
    python3 tools/pass39_independent_replay.py evidence/gcc evidence/independent-gcc.csv
    python3 tools/pass39_visualize.py evidence/gcc visuals

Use separate compiler-specific build and evidence directories. Recorded
timings used strict C++23 -O2 without fast math in a shared Linux VM;
Release CMake timings can differ. No production whole-solver acceleration
or GPU performance conclusion is supported.


## Passes 39C–39E — final physical campaign execution boundary

- [x] Verify upstream custom AABB constructor, LBVH build overload and
  index types at exact IPC Toolkit pin 19bcab7e1f5a0d2addfcd28513422ff248ea504e.
  The upstream lbvh.cpp explicitly outward-expands double bounds into
  float nodes. Normalize conservative extra candidates with the original
  binary64 predicate; keep the independent exhaustive equality guard.
- [x] Locally build and run external filter tests with GCC and Clang,
  strict C++23 warnings as errors: 1,000 randomized supersets plus
  binary64 precision, duplicate/incident/malformed/missing candidate
  controls. Filter cannot synthesize an absent candidate.
- [x] Final campaign runner independently preserves failure, build,
  timeout and device-blocked status; executes native/exact/IPC/Metal
  regression runner, paid persistence, all width scales, floor replay
  and independent validation, visualizations. Failure fixture and
  Python byte-compilation passed; actual full campaign unrun.
- [x] Accepted/rejected coupled-leaf visualization script validates
  source rows and event receipts, labels uncommitted trials and
  explicitly crops remote geometry. Four synthetic schema smoke
  GIF/PNG pairs rendered successfully (NOT physical results).
- [ ] Actual upstream IPC executable build/run, real coupled solver
  and Metal hardware; still not validated. No new upstream speed
  or correctness result is inferred from the filter unit test.
- [ ] Independent full external contact dynamics solver integration,
  which is beyond replacing a candidate producer in native FEM.
- [ ] Run final campaign on actual Mac checkout and retain the raw
  receipts. A successful campaign means the listed gates passed,
  not a universal safety or novelty proof.

    python3 tools/run_final_campaign.py --output /tmp/kyoto-final-$(date +%Y%m%d-%H%M%S) --width 64

Use `--without-metal` only to explicitly record an incomplete CPU
campaign. The manifest records the source revision and logs. On macOS
install CMake, Boost, Python NumPy/SciPy/pandas/Matplotlib/Pillow and
ffmpeg; the IPC dependency is fetched by CMake. Actual data must be
examined after each run; no result should be guessed from a target name.

## Pass 40 — technical manuscript and final evidence boundary

- [x] Complete technical manuscript with sufficient pair and whole-complex
  injectivity theorems, exact arithmetic formulation, reproducibility,
  measured experimental results, limitations, references and related work.
- [x] Review original IPC, Bernstein sign classification, Tight Inclusion,
  TightCCD, exact root parity and scalable CCD sources. The combination
  is a research direction, not an established first-in-literature claim.
- [x] Produce local 13-page rendered/inspected PDF and raw archive with
  both compiler datasets, previous exact audit measurements, source subset,
  native floor-contact figures and animations, and verified checksums.
  The raw archive is delivered separately; it is not silently replaced by
  summary tables. Markdown Git blob matches the local manuscript.
- [ ] Final research acceptance: actual coupled CPU/GPU campaigns,
  upstream IPC execution, independent full mesh-contact dynamics
  integration, real coupled source figures, and direct related-work
  dataset comparisons. Full native checkout unavailable and CI pending.
  Keep the validation PR draft. No claim that these unexecuted gates passed.

The exact-arithmetic milestone (39B) is closed; the physical floor-contact
CPU evidence, independent numerical replay and its visualization are
complete at their stated scope. The technical manuscript is complete as
a draft; submission-ready scientific validation remains open.

Read `docs/manuscript/SCA_MANUSCRIPT.md` and
`docs/evidence/PASS39_PHYSICAL_EVIDENCE.md`. The one-command integrated
runner is `tools/run_final_campaign.py`; an exit status of zero means
its listed executable gates passed, not general novelty or safety.

## Executed coupled FEM campaign — physical CPU gate now tested

- [x] Recover exact pinned source files (base c5e33ff61cb175cc671fedef6ec8f5944c665bd9) by connector
  transfer; 46 source entries verified against Git blob SHA. Local
  GCC 14.2 / Clang 17, strict C++23 -O2 -Wall -Wextra -Wpedantic
  -Werror -ffp-contract=off -fno-fast-math. Correct initializer ordering
  in RefitHierarchy (no change to actual member initialization order).
- [x] Twelve native regression executables passed on each compiler:
  coupled FEM, typed connected self contact, nonincident self contact,
  persistent context, initial containment, exact static endpoint,
  exact swept accepted leaves, coupled torus, native fixture topology,
  typed audit, linear adapter and coupled normal impulse.
- [x] Paid persistence: five ABBA/BAAB blocks (20 measured trajectories,
  eight macrosteps each), all physical parity checks passed. Whole-step
  cold/warm geomean GCC 1.034554x, Clang 1.063909x (shared Linux VM).
- [x] Full five-arm CPU campaign: widths 1/4/16/32/64, both near and
  separated, four accepted leaves each, 1,000 timed queries per compiler.
  All candidate identities match exhaustive. Near-contact impulses
  129/424/1299/2280/4077; separated zero. Max 1920 U tets + 48 remote.
- [x] Stronger eight-arm physical replay on same ten workloads and
  40 accepted leaves: add persistent x SAP, per-query projected-overlap
  axis selection, and persistent SAP with first-frame selected axis.
  Fix OrderedSweep cold path to use sort as documented. Eight rotated
  measured blocks, 2,560 queries per compiler; exact parity on every
  query. Retained refit / persistent selected-axis SAP runtime ratio
  GCC 1.038, Clang 1.003; frontier 1.058/1.072. At width64 persistent
  selected-axis SAP is faster than both retained hierarchies on this
  workload. The earlier win against fixed-x SAP does NOT establish
  superiority against a strong baseline.
- [x] Independent exact combined-volume audit of 52 actual accepted
  leaves (40 U scaling, 12 cross-torus) per compiler, 799,768 exact
  pair-path certificates, 20,848,440 possible pairs, zero unresolved
  accepted leaves. Strata 364648/264752/121744/48624 (0/1/2/3 shared
  vertices). Cross-torus 1073 impulses. Deliberate rejected cross-body
  proposal fails certificate and independent exact static endpoint
  oracle confirms overlap; zero-subdivision step rolls back clocks.
  GCC and Clang full audit CSVs byte-identical.
- [ ] Official pinned IPC execution, external mesh-contact integration,
  real Metal execution. No GPU device; IPC dependency unavailable.
  CPU result is actual coupled native execution, not floor replay.

Reproduce from repository root:
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass39_strong_cpu_baselines.cpp -o /tmp/strong
    /tmp/strong /tmp/strong-results 64
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass39_coupled_volume_campaign.cpp -o /tmp/coupled-volume
    /tmp/coupled-volume 64 > /tmp/coupled-volume.csv

The raw timing files are retained in a separate downloadable local evidence
archive; the repository summary is rounded to three decimals. Timing
ratios are paired per block, equal-weight geomeans, not whole-solver or
universal claims. Exact volume receipt SHA-256:
20d88849b0b019f2ffd6de848f998caf00d77583fb268dff42c0ad8bf75a9c27.

## Executed independent upstream narrow-phase validation

- [x] Retrieve hash-verified actual Tight Inclusion v1.0.6 numerical
  source, the exact version specified in pinned IPC Toolkit's
  cmake/recipes/tight_inclusion.cmake (IPC pin 19bcab7e1f5a0d2addfcd28513422ff248ea504e;
  upstream v1.0.6 tree 7377d78162f9fdb5fe44b54b6f9c01bc6ce0885a).
- [x] Compile original ccd.cpp, interval.cpp, interval_root_finder.cpp,
  avx.cpp separately on GCC and Clang, using available Eigen and
  default double precision, timers/rational/queue limit disabled.
  Missing spdlog was replaced only in the local build copy with
  a logging shim (trace no-op; upstream error throws); original
  numerical source unchanged. No performance result attributed to
  the external dependency. Full official IPC wrapper/LBVH not run.
- [x] Exhaustive admissible VF/EE enumeration without broad-phase
  culling, for 20 accepted native leaves and 2 rejected native
  proposals, tested at both zero and 0.0005 m separation.
  Tight Inclusion BFS, auto error bound, tolerance 1e-7 m,
  max 100000 iterations, tmax=1, no_zero_toi=false.
  1,192,226 queries per compiler, 1,156,128 on accepted leaves
  (361,344 VF and 794,784 EE), no reported possible contact on any
  accepted leaf and zero missing candidate keys on all queries.
- [x] Positive controls: rejected U-solid proposal reports 57
  self-event stencils, rejected cross-tetra proposal reports 42
  cross-event stencils, each at both separations. Both actual
  zero-subdivision native steps refuse and leave clocks unchanged.
  TOI values are conservative prefixes, not exact impact times.
  All GCC and Clang raw receipts byte-identical; SHA-256
  c30e8a0f08d1cc51567a57dec4210d76fcf508c0f4f2b777daba0dbd8bf38c85.
- [x] Register direct test against actual CMake dependency when IPC
  enabled; source and raw exact receipts hash-verified.
- [ ] Full IPC Toolkit API and LBVH integration still unexecuted.
- [ ] Real GPU hardware not available (CUDA/MPS false; no GPU devices).
- [ ] Independent full external contact dynamics solver, beyond
  narrow-phase validation.

This is actual independent external CCD execution, not a substitute
solver built from our own equations. It strengthens the empirical
safety evidence while leaving broader interoperability gates open.

## Executed performance follow-up — cached sweep and scalar interval specialization

- [x] Add topology-validated cached feature lookup to persistent selected-axis
  SAP. Recompute all bounds; no candidate membership cache. Bounded order
  repair falls back to full sort after 8N swaps. Test 4,000 changing-mesh
  queries per compiler, 500 malformed/recovery controls, sparse IDs,
  masks, reordered source rows, epoch changes, and severe reversal
  (two fallback sorts). Exact exhaustive candidate parity.
- [x] Nine-arm physical replay at all five widths and both regimes,
  nine rotated measured blocks, four accepted leaves per case,
  3,240 timed queries per compiler, all canonical keys match exhaustive.
  Persistent selected-axis SAP / cached SAP timing ratio GCC 0.984903,
  Clang 1.094739. Mixed result; not a portable speedup.
- [x] Integrate cached sweep as an explicit typed-self backend in the
  actual solver (default off). Full state/receipt parity after every
  physical macrostep, 120 measured trajectories of four macrosteps per
  compiler. Verify context backend switching and failure recovery.
  Retained BVH / cached SAP whole-solver ratio GCC 0.9964636076,
  Clang 1.0004891200. No demonstrated whole-solver speedup.
- [x] Add optional exact-scalar specialization for outward interval
  multiplication in relative triangle certificates. A point interval
  multiplier has only two distinct endpoint products; outward bounds
  and failure semantics remain unchanged. Test 200,000 random finite
  binary64 interval/scalar products and explicit subnormal/signed-zero
  controls per compiler; 2,000 full certificate comparisons per compiler
  (1,365 clear, 635 unresolved) match all receipt fields exactly.
- [x] Actual full solver comparison with reference versus specialized
  projection, 120 measured trajectories of four steps per compiler,
  all state and physical decision receipts identical. Reference/fast
  geomean GCC 1.1384441884 and Clang 0.9869305001. GCC improves 13.8%
  in speed ratio, Clang slightly regresses. Both options default off.
  The legacy triangle certificate and restricted nonincident self
  contact path are unchanged. No new safety claim.
- [x] Record four raw whole-solver timing CSVs and summary. Initial
  candidate timing was measured before adding the redundant explicit
  thickness validation and optional integration; subsequent final
  candidate source builds pass, but timings refer to that initial run.
  Shared Linux VM, strict C++23, paired block sums and equal-weight
  geometric means over workload/block combinations. No independent
  hardware replication or universal superiority.
- [ ] Full official IPC wrapper/LBVH and hardware Metal execution.
  Latest CI remained pending/queued. No GPU result claimed.

Run from checkout with -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror
-ffp-contract=off -fno-fast-math -Iinclude. Use the registered unit
tests and apps/pass39_cached_sap_campaign.cpp (output directory and 64),
apps/pass39_cached_sap_solver.cpp and apps/pass39_projection_solver.cpp
(stdout CSV). Preserve separate compiler runs and warmup exclusion.

## Executed longer cross-contact campaign — within-call witness reuse

- [x] Six native physical workloads, 20/24 macrosteps per trajectory:
  aligned 96-tet torus pair, oblique translated 96-tet pair, rotated
  240-tet pair, separated 96-tet pair, 168-tet U/remote pair and
  rotated 528-tet U/remote pair. All use the actual coupled solver.
- [x] Larger rotated case exposed a real pre-existing Voronoi
  barycentric failure: (-7.4081044109974115e-17,
  0.99999999999999944, 6.2919255642255239e-16). Add bounded
  64-epsilon excursion repair, clamp/renormalize/recompute witness
  only when needed; larger excursions remain fail-closed. Exact
  captured point and triangle regression passes GCC and Clang.
  This is floating-point response robustness, not an exact
  point-triangle distance proof. Both benchmark arms share the fix.
- [x] Optional reuse_cross_geometry caches geometric face-pair
  witnesses only inside an impulse call, preserving canonical face
  iteration and recomputing velocity-dependent impulses every pass.
  No reuse across drift or source changes; default remains off.
- [x] GCC/Clang strict C++23 O2, warnings-as-errors, no fast math.
  Three measured ABBA/BAAB blocks per scene, excluded warmup.
  72 measured trajectories / 1632 macrosteps per compiler; every
  nodal coordinate, velocity, impulse, time, contact count and
  listed safety/decision receipt equal across arms after each step.
  Cross impulses per trajectory 2818/100/5564; self impulses
  1195/3920; separated control has zero impulses. All steps accepted.
  Prior GCC coupled FEM, typed self-contact and torus tests passed;
  Clang coupled FEM passed before the regression batch timed out.
  Captured barycentric regression passed on both compilers.
- [x] Paired block-sum reference/reuse speed ratio (higher is better):
  GCC 1.142454x overall, 1.319180x on three active cross-body cases;
  Clang 1.158695x overall, 1.335657x on active cross-body cases.
  Aligned impact 1.674698/1.668208, oblique 1.120061/1.146934,
  larger rotated 1.223865/1.245365, separated .959390/1.007024,
  U self 1.006699/1.008256, rotated U 1.002824/1.000274.
  Actual whole-solver timing, including candidate and safety checks;
  shared Linux VM, no hardware-independent inference. Cross-contact
  reuse is a response-stage optimization, not a new broad-phase win.
- [ ] Full upstream IPC wrapper/LBVH and real Metal still unexecuted.
  Long trajectories have native safety certificates; no new
  independent upstream/exact whole-volume audit of these paths yet.

Reproduce with:
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass39_cross_witness_reuse.cpp -o /tmp/cross-reuse
    /tmp/cross-reuse > /tmp/cross-reuse-gcc.csv

Raw source-hash-verified CSVs: docs/evidence/pass39_cross_witness_{gcc,clang}.csv.

## Executed long-trajectory independent safety verification

- [x] Re-run all six physical workloads from the cross-witness experiment with both reference and optimized contact response, comparing complete before/after leaf states and time/depth, not just committed macrosteps. Both modes produced identical physical endpoints throughout.
- [x] Independent exact swept-volume audit of each combined two-body complex and exhaustive admissible Tight Inclusion VF/EE checks at zero and 0.0005 m clearance. Same original upstream v1.0.6 numerical library as earlier direct audit, local logger shim only; no IPC wrapper or LBVH execution claim.
- [x] GCC 14.2 and Clang 17 strict C++23 O2, warnings-as-errors, no fast math, using prior verified source subset and the latest two changed headers. 136 accepted leaves per compiler, 4,079,760 possible tetrahedral pairs, 433,291 exact pair-path certificates (171547/139408/84080/38256 by shared vertices 0/1/2/3), 27,544,656 upstream CCD queries per compiler. Zero unresolved exact pair/orientation checks, zero reported upstream events at either clearance and zero missing candidates. Full CSV byte-identical; SHA-256 ae1217f8e3702d3c1ce7dd3ef345e85eb5f94e11925a0b05c157f3c04ebcb447. Clean rerun into private temporary files after live CSV export interfered with initial output; only complete rerun accepted.
- [x] Register optional integrated test when both exact and official IPC dependencies are enabled. Local direct compiler build/run verified; complete CMake target still requires dependency checkout.
- [ ] Real Metal, complete IPC Toolkit wrapper/LBVH and independent external contact dynamics remain unexecuted.

The audit is outside the timed solver path. It corroborates the tested physical trajectories, not universal solver injectivity or a new performance result. Raw receipt: docs/evidence/pass39_long_physical_audit_gcc_clang.csv.

## Executed initial cross-volume sweep experiment

- [x] Conservative closed-AABB bipartite sweep with inclusive endpoint tie order, best projected cross-overlap axis, canonical pair order, and finite/ordered input validation. No same-body pairs generated.
- [x] 2,000 seeded random bipartite scenes per compiler, 30,087 matching candidate identities, 4,582,075 exhaustive versus 804,122 three-axis checks. Touching/zero-width and malformed controls passed on both.
- [x] Optional initial_cross_sweep solver integration; same body-level rejection, same strict initial tetrahedron certificate, same canonical order; no skip/reuse of geometric certification. Existing initial-volume regression runs both modes including containment and incomplete-boundary rejection, oblique safe overlap of bounds, and remote fast path. Both compilers passed.
- [x] Six sustained physical workloads, three paired ABBA/BAAB measured blocks plus discarded warmup. Reference and sweep both enable within-call cross witness reuse. 72 measured trajectories, 1,632 macrosteps per compiler, identical nodal states and all listed physical and certificate receipts. Rotated torus initial-volume checks 345,624 -> 134,803 per trajectory (24,247 exact separating-plane certificates unchanged); oblique torus 55,320 -> 28,968 (9,528 certificates unchanged); four body-disjoint workloads retain the fast path. Reference/sweep whole-solver geometric mean GCC 0.9990062557, Clang 1.0007277788. No measurable end-to-end win; retain default off. Shared VM and some overlapping regression compilation; small timing differences are not evidence of superiority.
- [x] Re-run previously incomplete longer-trajectory regression batch: barycentric, typed connected self contact, coupled torus, persistent context, native exact swept volume, both compilers, all ten executions passed before the new optional sweep change. Long physical audit results in previous commit apply to reference and witness-reuse modes without this new sweep. Sweep campaign independently checks equal physical states and certificate work.
- [ ] Real Metal hardware, full IPC wrapper/LBVH, independent external dynamics solver remain outstanding. CI still pending/queued.

Raw paired timings in docs/evidence/pass39_cross_volume_sweep_{gcc,clang}.csv. Direct builds used strict C++23 -O2, warnings-as-errors, -ffp-contract=off -fno-fast-math -Iinclude. CMake registration authored, full CMake dependency graph not run.

## Executed pinned IPC LBVH and public CCD wrapper integration (serial dependency adaptation)

- [x] Recover pinned IPC Toolkit 19bcab7e1f5a0d2addfcd28513422ff248ea504e source subset with verified Git blob hashes. Compile actual LBVH, AABB, candidate, distance, closest-point, normal and TightInclusionCCD wrapper implementations using installed Eigen and prior pinned Tight Inclusion v1.0.6 numeric library. Local serial TBB API compatibility layer and local logging shim because oneTBB/spdlog unavailable, generated config disables SIMD and optional integrations. Only the exact upstream Candidates::clear definition is extracted from candidates.cpp; unused methods not linked. This is a functional upstream source-subset build, not a stock full IPC CMake build or parallel timing result.
- [x] Three existing native integration tests on GCC and Clang: LBVH parity (13 scenes, 36,799 exact candidate identities, zero missing), actual native external producer (64 calls, 8 leaves, 208 self impulses, corrupt producer rollback), accepted/rejected IPC wrapper audit (10 accepted leaves, 143,900 admissible stencil checks, 57 self and 42 cross event stencils detected on rejected trials, zero misses). All logs and Pass34 exported CSVs byte-identical.
- [x] Long native campaign on six physical workloads: 136 accepted leaves per compiler, 272 rows at zero and 0.0005 m clearance, exhaustive upstream wrapper enumeration on first and final macrosteps (24 rows), normalized upstream candidate checks on remaining rows. 129,157 matching candidate identities, 2,680,890 CCD wrapper queries (809,273 VF and 1,871,617 EE), zero reported possible contact and zero misses. On both U self-contact cases, actually replace the native self-contact producer with upstream LBVH (320 calls); all compared nodal, clock, physical decision and before/after leaf receipts match. Cross-body cases use the native integrator and audit the upstream candidate set externally, not replace the cross-contact response. Both compiler receipts byte-identical, SHA-256 dab3890cc836bcd1d742aaf1f476ee975e764897a91c304e370b1cb4e1152e7d.
- [x] Register the new long test against normal pinned ipc::toolkit target when dependency enabled. Actual source and measurement CSV Git hashes verified; CMake registration authored but stock dependency build still unexecuted.
- [ ] Actual upstream parallel oneTBB/SIMD stock build; independent external mesh-contact dynamics solver; real Metal hardware. No performance claims against upstream serial compatibility build. Further broad-phase superiority is not established.

Raw receipt: docs/evidence/pass39_long_ipc_wrapper_gcc_clang.csv. Local execution archive includes upstream source subset, exact configuration and shims, logs, native source and raw receipts.

## Independent contact dynamics and packed GPU mask execution

- [x] Independently formulate compressible neo-Hookean implicit Euler FEM via CasADi 3.7.2/IPOPT. A fixed separating direction and an optimized moving plane impose a positive 1.5 mm gap between two deformable blocks, while positive Jacobian constraints guard orientation. Different integrator/contact law from native impulse solver. Six actual 32-step simulations at 1 ms; 12/48/96 total tetrahedra, aligned/rotated impacts and separated control. Five nonvacuous active-contact runs; minimum J 0.970788357, maximum stationarity residual 8.1222e-14, complementarity 9.8151e-11, total momentum error 2.6037e-17 kg m/s. No claim of general nonplanar contact, self-contact, or native trajectory equality.
- [x] Replay all 192 independent steps through native exhaustive/SAP and pinned upstream LBVH and public CCD wrapper (same serial dependency adaptation described above). Two clearances, all admissible VF/EE stencils exhaustively enumerated: 2,218,752 queries, 36,345 matching candidate identities, 114,948 exact volume pair-path certificates, zero unresolved certificates or possible contact/missing keys per compiler. GCC/Clang strict C++23 O2 receipts byte-identical. The input trajectory SHA-256 is 6cce7c1b64a821b015682dcfe13380a7d5a78d208ce5a4bc015140aadc1e9cbd; full trajectory is in local evidence archive and reproducible from the generator.
- [x] Shared packed GPU mask body, binary64 outward float enclosure and exact canonical host filter. One word for 32 flags instead of one word per pair. Same scalar source emitted to OpenCL and Metal. Clang OpenCL C compiled to SPIR64 LLVM bitcode and x86-64 object; actual compiled kernel executed by serial host work-item harness with get_global_id shim. GCC and Clang host code separately compiled and passed. 1,000 randomized scenes per compiler with tiny/large coordinates, masks, sparse IDs, odd chunk lengths, padding guard and overflow controls; 40 actual native physical accepted leaves across all five U widths and both contact regimes. 70,901,664 rectangular pair slots, 2,215,728 words, 328 dispatch batches and 347,680 float mask positives per compiler. Exact candidate parity; raw receipts byte-identical. Logical mask payload 283,606,656 -> 8,862,912 bytes (~32x); not measured transfer time, GPU speedup or device execution.
- [x] Author separate Metal benchmark comparing original mask, packed mask and persistent selected-axis SAP, six order-balanced blocks; retain original benchmark. Metal host source and CMake target not compiled or run on this Linux environment. No Metal or OpenCL device platform (clGetPlatformIDs returned -1001), no actual GPU timings.
- [ ] Broader independent contact solver generality, stock parallel IPC and actual GPU hardware execution. Do not label GPU milestone complete from offline compilation.

Reproduce independent dynamics:
    OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python3 tools/pass40_ipopt_contact.py /tmp/kyoto-ipopt
    cmake -S . -B build-ipopt -DKYOTO_WITH_OFFICIAL_IPC=ON -DKYOTO_WITH_EXACT_SELF_VOLUME=ON
    cmake --build build-ipopt --target kyoto_pass40_ipopt_replay
    ./build-ipopt/kyoto_pass40_ipopt_replay /tmp/kyoto-ipopt/trajectory.txt > /tmp/ipopt-replay.csv

Offline shader execution (Linux x86-64, Clang OpenCL C compiler):
    g++ -std=c++23 -O2 -Iinclude tools/export_gpu_shader.cpp -o /tmp/export-gpu
    /tmp/export-gpu > /tmp/packed-mask.cl
    clang -x cl -cl-std=CL1.2 -target spir64-unknown-unknown -emit-llvm -c -O2 -Werror /tmp/packed-mask.cl -o /tmp/packed-mask.bc
    clang -x cl -cl-std=CL1.2 -target x86_64-unknown-linux-gnu -c -O2 -Werror /tmp/packed-mask.cl -o /tmp/packed-mask.o
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/pass40_gpu_packed_mask_tests.cpp /tmp/packed-mask.o -o /tmp/packed-mask-test
    /tmp/packed-mask-test > /tmp/packed-mask.csv

Real Mac device gate (unrun):
    cmake --build build --target kyoto_pass40_metal_packed_mask
    ./build/kyoto_pass40_metal_packed_mask /tmp/packed-metal 64

Raw evidence CSVs in docs/evidence/pass40_*.csv.

## Executed Pass 40 follow-up: stronger CPU sweep, three-body implicit contact and tiled shader

- [x] Spatial-first optional cached sweep: perform original inclusive binary64 AABB overlap before mask/incident checks. Default remains the previous order. All 4,000 changing-mesh comparisons and 500 malformed/recovery controls per compiler passed, including reversal fallback. Candidate counts agree; work-counter semantics intentionally differ by filter order.
- [x] Paid matched replay: six sustained coupled workloads plus ten width/regime cases (176 accepted leaves, 16 workloads); six order-balanced measured blocks, three arms, 3,168 queries per compiler, all candidate identities equal to exhaustive. Reference persistent-axis SAP / spatial-first geometric mean 1.3912495283 (GCC) and 1.2193532409 (Clang). Prior cached SAP / spatial-first 1.2885464520 and 1.0971780037. One Clang case regressed versus persistent axis. Measured query includes complete preparation and filtering; this does not establish universal superiority.
- [x] Optional solver integration with independent context invalidation, spatial-first requiring cached SAP. Three order-balanced measured blocks on ten native width/regime cases; 90 trajectories and 360 macrosteps per compiler, equal physical state and tested decision receipts; switches and rollback passed. Retained BVH / spatial-first whole-solver geomean 1.0069077624 (GCC) and 1.0342789132 (Clang). Cached SAP / spatial-first 1.0372045301 and 1.0345253073. Shared VM; small whole-solver gains must not be overstated. Timeouts were resolved by rerunning only missing complete workload cases; combined data validated for all slots and no duplicates. Benchmark source gained a minimum-case selector for resumption; timed inner loop unchanged.
- [x] Independent CasADi/IPOPT three-body dynamics, three pairwise fixed directions and independently optimized moving separating planes. Four 32-step trajectories, three active cases (16 simultaneous two-contact steps each) and one separated control. Positive Jacobians (minimum approximately .977058), stationarity residual at most 8.04e-14, complementarity at most 6.14e-11. No claim of arbitrary nonplanar contact or independent self-contact dynamics. Replay each of 128 accepted steps at both zero and .0005 m clearance: 740,736 exhaustive upstream public CCD queries, 18,098 matching candidate identities, 18,324 exact swept cell-pair certificates per compiler. No reported events or missing candidates, zero unresolved certificates. GCC and Clang receipts byte-identical. Upstream IPC remains the previously validated serial compatibility build, not stock oneTBB.
- [x] Tiled packed GPU work planner: exact binary64 union bounds of groups of 32 features sorted on selected axis, reject disjoint tile pairs, preserve every inclusive candidate via outward float masks and original binary64 host filter. Shared shader compiled to SPIR64 LLVM IR and host-native OpenCL C object; actually execute compiled kernel in serial CPU work-item harness. GCC/Clang each passed 1,000 randomized scenes and 40 native leaves, including all five width scales and two regimes. Physical logical rectangular slots 70,901,664 -> 4,861,344 (93.14% fewer, including diagonal tile reserved slots); output words 2,215,728 -> 154,880 (8,862,912 -> 619,520 bytes, 14.31x smaller). 4,840 jobs and 42,176 coarse tile overlap tests across distinct leaves, 103,888 final candidate keys. Same receipts across compilers, excluding timing. 360 timed CPU queries per compiler across three arms. Persistent SAP / tiled CPU-emulation geomean .7066727351 and .6649408962: slower on CPU. These are logical workload and payload reductions, NOT GPU speedups. Emitted Metal shader has not been compiled or integrated into a new real-device host. GPU hardware unavailable.
- [x] Execute actual accepted and rejected native IPC leaf visualizations from previously measured source, plus recorded independent three-body trajectory video and impulse plot. Raw source and animation files delivered in a separate evidence archive; renderer source registered in repository. No manuscript work.
- [ ] Actual GPU device, real Metal compilation and paid GPU comparisons; stock upstream parallel IPC build; broader independent arbitrary contact and adversarial physical generality. No formal percentage acceptance claim.

Reproduce new CPU tests (C++23 strict floating point) via the added CMake targets or direct -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude compilation. Spatial-first solver args are optional maximum width and minimum case ID; use default for full workload. Query campaign optional minimum scene ID (default all). Raw query receipts are gzip-compressed in docs/evidence; decompression is lossless.

Independent contact:
    OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python3 tools/pass40_three_body_contact.py /tmp/three-body
    cmake -S . -B build-three -DKYOTO_WITH_OFFICIAL_IPC=ON -DKYOTO_WITH_EXACT_SELF_VOLUME=ON
    cmake --build build-three --target kyoto_pass40_three_body_replay
    ./build-three/kyoto_pass40_three_body_replay /tmp/three-body/trajectory.txt
    python3 tools/render_three_body_contact.py /tmp/three-body /tmp/three-body-visuals

Offline shader (Linux x86-64, Clang OpenCL C compiler):
    g++ -std=c++23 -O2 -Iinclude tools/export_tiled_gpu_shader.cpp -o /tmp/export-tiles
    /tmp/export-tiles > /tmp/tiles.cl
    clang -x cl -cl-std=CL1.2 -target spir64-unknown-unknown -emit-llvm -c -O2 -Werror /tmp/tiles.cl -o /tmp/tiles.bc
    clang -x cl -cl-std=CL1.2 -target x86_64-unknown-linux-gnu -c -O2 -Werror /tmp/tiles.cl -o /tmp/tiles.o
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude tests/pass40_tiled_gpu_mask_tests.cpp /tmp/tiles.o -o /tmp/tiles-test
    /tmp/tiles-test > /tmp/tiles.csv

CMake registrations authored; full stock dependency configuration and physical GPU run remain unverified.

## Executed adaptive separating normals and fused active-list sweep

- [x] Independent CasADi/IPOPT three-body solver extends the fixed-plane
  formulation to optimized unit separating normals for each pair at each
  step. Normal-only 1e-8 proximal regularization selects nearby feasible
  directions; spin is added to active workloads. Four 32-step scenes:
  three rotating impact cases and separated control. All 128 implicit
  steps converged, 48 simultaneously active steps, largest normal
  change from initial 9.946124 degrees, minimum element J 0.956324478,
  maximum unit-normal error 4.564e-11, stationarity 8.158e-11,
  momentum error 7.035e-11 kg m/s. Positive 1.5 mm separating gap.
  Contact model remains pairwise globally separating planes; cannot
  claim arbitrary nonconvex surface contact or external IPC dynamics.
- [x] Existing GCC/Clang three-body replay executables independently
  audited the new trajectories at 0 and 0.5 mm clearance. Per compiler:
  128 accepted linear leaves, 740,736 exhaustive public upstream CCD
  queries, 20,024 matching candidate keys, 18,668 exact pair-path
  certificates (count once per leaf), zero unresolved or reported
  collision/missing candidates. Raw receipts byte-identical. IPC is
  the previously disclosed serial compatibility build.
- [x] Fused active-list SAP merges expiration and candidate enumeration,
  lazily compacts only the opposing VF active list, and retains the
  original spatial-first exact binary64 filter. Independent tests on
  GCC/Clang strict C++23: 4,000 changing meshes each, 500 malformed
  recovery checks, reversal fallback and empty invalid input controls.
  Never caches candidate membership.
- [x] Paid query comparison across eight independent 32-step contact
  trajectories (four moving normals, four previous fixed normals) and
  ten native four-step U-width/near-separated workloads. 296 distinct
  physical leaves per compiler, six rotated measured blocks and
  excluded warmup, three methods, 5,328 timed queries per compiler.
  All candidate identities match exhaustive. Equal-weight paired
  block-sum geometric means: persistent selected-axis SAP / fused
  GCC 1.595133, Clang 1.466904; spatial-first / fused 1.170239 and
  1.144211. Independent-only persistent/fused 1.737172 and 1.627156;
  native-only 1.489910 and 1.350144. One GCC workload regresses
  against persistent axis SAP, one slightly regresses against
  spatial-first. Shared Linux VM timings; no universal or whole-solver
  performance claim. Fused backend is a separate research query class,
  not yet integrated into the physical solver.
- [x] Author actual tiled Metal host using existing packed and tiled
  shared kernels, command completion, precision controls, exact key
  comparison, and measured planning/upload/dispatch/readback/filter
  against spatial-first SAP. Six rotated blocks, five native width
  scales. Metal host remains uncompiled/unexecuted on Linux. No actual
  GPU performance evidence; no GPU device or OpenCL platform.
- [ ] Stock upstream parallel IPC, real GPU execution, broad arbitrary
  surface-contact dynamics, and fused full-solver integration remain.

Reproduce adaptive contact:
    OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python3 tools/pass40_adaptive_contact.py /tmp/adaptive-contact
    ./build/kyoto_pass40_three_body_replay /tmp/adaptive-contact/trajectory.txt
    ./build/kyoto_pass40_fused_axis_campaign /tmp/adaptive-contact/trajectory.txt /tmp/three-body/trajectory.txt

Physical Metal gate (unrun):
    cmake -S . -B build-metal -DCMAKE_BUILD_TYPE=Release
    cmake --build build-metal --target kyoto_pass40_metal_tiled_mask
    ./build-metal/kyoto_pass40_metal_tiled_mask /tmp/kyoto-tiled-metal 64

Raw timings are losslessly gzip-compressed. Full independent trajectory
and logs retained in the local evidence archive. Direct strict compiler
builds and native replays executed; new CMake registrations authored.

## Executed fused whole-solver comparison and independent nonconvex contact

- [x] Integrate FusedAxisSweep as opt-in typed-self candidate backend. Require cached and spatial-first flags; preserve original default. Context invalidates on backend switch and failed transaction. Actual GCC/Clang strict C++23 O2, warnings as errors, no fast math. Four arms, four rotated measured blocks, ten physical cases, four macrosteps each, excluded warmup. 160 timed trajectories and 640 measured macrosteps per compiler. Every compared state and listed physical/certification receipt matches after every step. Backend switching and rollback passed. Equal-weight paired block geomean reference/fused whole solver: retained BVH GCC 1.019920 / Clang 1.017664; cached SAP 1.015135 / 1.019547; spatial-first 1.007892 / 1.014807. Small gains, shared Linux VM; no universal superiority or portable hardware claim.
- [x] Independent CasADi/IPOPT implicit neo-Hookean FEM for connected concave U (42 tetrahedra) and inserted cube (6 tetrahedra). The insert starts strictly inside convex hull of the connected U, making a single global separating plane impossible. Three local groups cover all U cells; optimized per-region unit normals and planes constrain contact against the insert. Four 32-step cases, including rotation, narrower clearance and stationary control. All 128 steps converged, 32 simultaneous two-region contact steps. Minimum J 0.9736933, minimum gap 0.0015 m within tolerance; maximum total momentum error 1.367961e-10 kg m/s, stationarity 9.833160e-11, complementarity 3.106928e-9, normal rotation 0.762061 degrees. First run rejected wider insert hull-placement fixture (z touched hull); corrected z-centering and repeated all four cases. No arbitrary automatic nonconvex solver or self-contact response claim.
- [x] Exhaustive independent replay of all 128 accepted linear paths on GCC and Clang, explicitly distinguish unequal body node counts. Native exhaustive/SAP and normalized pinned upstream LBVH agree, 37,326 candidate identities per compiler across two clearances. 2,032,128 upstream VF/EE checks per compiler (681,984 VF, 1,350,144 EE), 54,126 exact swept cell-pair certificates, zero unresolved, no possible CCD events or missing candidates. Compiler receipts byte-identical. Uses earlier upstream serial scheduling/logging adaptation, not stock parallel IPC.
- [x] Update real-device Metal tiled comparison to include both spatial-first and fused SAP, packed and tiled Metal with eight rotated measured blocks. Source authored; hardware compile/run remains unverified. No GPU platforms available (OpenCL -1001, CUDA/MPS false, no device nodes). Do not equate offline CPU execution with GPU execution.
- [ ] Stock parallel IPC build and real Metal device measurements, broader automatic contact constraints and further adversarial/generalization validation. No claim of total research completion.

Reproduce:
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass40_fused_solver.cpp -o /tmp/fused
    /tmp/fused 64
    OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python3 tools/pass40_nonconvex_contact.py /tmp/nonconvex
    cmake -S . -B build-closeout -DKYOTO_WITH_OFFICIAL_IPC=ON -DKYOTO_WITH_EXACT_SELF_VOLUME=ON
    cmake --build build-closeout --target kyoto_pass40_nonconvex_replay
    ./build-closeout/kyoto_pass40_nonconvex_replay /tmp/nonconvex/trajectory.txt

Actual Mac device gate (unrun):
    cmake -S . -B build-metal -DCMAKE_BUILD_TYPE=Release
    cmake --build build-metal --target kyoto_pass40_metal_tiled_mask
    ./build-metal/kyoto_pass40_metal_tiled_mask /tmp/kyoto-metal-new 64

The raw paired timings, independent physical receipts and replay CSV are committed under docs/evidence. CMake registration authored but stock dependency build not run.

## Physical Mac results reported by operator; audited tiled producer integration

- [x] Operator reports M2 Pro physical tiled/packed Metal query campaign
  at ae962885738a5676e73941c7c4ce018f2401149e: 1280 observations,
  320 matched four-arm groups, all five widths and two regimes, exhaustive
  candidate-vector parity. Fused CPU SAP has lowest median paid latency at
  every width in both regimes. Width 64 separated: fused 0.647 ms,
  spatial-first 0.841, tiled Metal 2.493, packed Metal 8.516; near:
  fused 0.780, spatial-first 0.943, tiled 2.544, packed 8.316 ms.
  Hardware measurement includes planning/dispatch/readback/filtering,
  not a full GPU solver. Do not assert a GPU performance advantage.
- [x] Operator reports stock pinned IPC Toolkit build (oneTBB, SIMD
  disabled), native backend CTest 1/1 and independent nonconvex replay
  128 accepted steps, 32 simultaneous-contact steps, 2032128 CCD
  checks, 54126 exact pair-path certificates, zero events/missing
  keys. One zero-skin candidate count 106 vs earlier 102, with differing
  regenerated gap and IPOPT iteration count; locally verified exhaustive
  parity. Other 255 rows matched. Preserve the cross-platform variation.
  Operator also reports actual nonconvex U/insert gallery, shared camera
  and scale, topology and local volume-change plots. Raw Mac files have
  not been transferred into this environment, so these are reported
  results, not independently inspected local artifacts.
- [x] Execute actual compiled OpenCL C tiled kernel through external
  candidate hook inside native coupled FEM against fused SAP reference
  (CPU-host machine code, NOT GPU hardware). All 34 transitive repository
  headers match pinned Git blobs. Strict GCC/Clang C++23 builds, freshly
  emitted shader compiled by Clang. 12 physical cases per compiler:
  all five widths and both regimes, plus rotated width4 near/separated.
  56 accepted leaves, 448 producer calls, 589120 matching candidate
  identities, 8780 self impulses, 22469696 tile slots, 22467 jobs,
  718944 output words per compiler. Every compared state and receipt
  matches, including before/after accepted leaf endpoints, depth and dt.
  Both receipts byte-identical, SHA-256
  8a0bd3c26afd6b45341fa7cb37a002abd230f4921a056cc22f6e4cc2687a381f.
- [x] Missing candidate injected into a nonempty actual backend result
  triggers exact exhaustive parity exception; truncated actual dispatch
  mask triggers exact size exception. Both preserve caller trace and
  committed states and clear context; subsequent recovery and switching
  external/fused/external verified. Exhaustive guard remains mandatory;
  this is an integration correctness test, not a paid speedup test.
- [x] Extract previously device-tested Metal dispatch class unchanged
  into common Objective-C++ header and add actual Metal frontend for
  identical native integration campaign. New frontend/refactor authored
  but not compiled or executed on Mac here; user-reported earlier
  query results refer to the previous source. Full device integration
  gate remains open until the new command passes.
- [ ] Actual device bridge execution and further generalization beyond
  tested decompositions; no claim of automatic general nonconvex contact
  or universal CPU/GPU superiority.

Reproduce host kernel integration (Linux x86-64):
    g++ -std=c++23 -O2 -Iinclude tools/export_tiled_gpu_shader.cpp -o /tmp/export-tiles
    /tmp/export-tiles > /tmp/tiles.cl
    clang -x cl -cl-std=CL1.2 -target x86_64-unknown-linux-gnu -c -O2 -Werror /tmp/tiles.cl -o /tmp/tiles.o
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fno-fast-math -Iinclude apps/pass40_tiled_solver_host.cpp /tmp/tiles.o -o /tmp/bridge
    /tmp/bridge 64

New actual Mac solver bridge (unrun here):
    cmake -S . -B build-metal -DCMAKE_BUILD_TYPE=Release
    cmake --build build-metal --target kyoto_pass40_metal_solver_bridge
    ./build-metal/kyoto_pass40_metal_solver_bridge /tmp/kyoto-metal-bridge 64

Raw host receipts: docs/evidence/pass40_tiled_solver_host_gcc_clang.csv.

## Pass 41 — integrated exact reference gate, unified baseline and one-command closeout

- [x] New opt-in exact_coupled_transaction::step stages the native bodies and leaves,
  independently certifies the initial combined tetrahedral complex and every
  provisional accepted linear leaf, then publishes both bodies/trace only if
  all certificates pass. Reset the warm context on any failure. Native
  timestep subdivision and physics remain unchanged; an unresolved exact
  path rejects the entire macrostep rather than being silently accepted.
- [x] Actual strict GCC/Clang C++23 -O2, warnings-as-errors, no fast math.
  Complete width-16 campaigns (nine workloads including rotated U near and
  separated and cross torus); 52 accepted macrosteps/leaves per compiler,
  191,558 exact leaf pair certificates and 189,348 initial certificates each,
  2,423 self and 1,073 cross impulses. Compared complete state and leaf parity.
  Deliberate same-body crossing with native self-contact disabled is accepted
  by ordinary native path but rejected by exact gate, as is initial contained
  tetrahedron; rollback preserves state/trace, then valid step recovers.
  GCC width64 attempt timed out after width64 separated; NOT a completed
  width64 exact gate. Median exact/native cost at width<=16 18.90x GCC,
  21.23x Clang: stronger expensive reference, not acceleration.
- [x] Unified common benchmark: eleven native CPU arms on identical fresh
  accepted native U snapshots and archived independently generated fixed,
  adaptive, and nonconvex trajectories. All five native widths/two regimes,
  12 independent and 10 native physical workloads, 424 distinct accepted
  leaves, 11 order-balanced blocks, 51,304 timed observations and 4,664
  matched groups per compiler; full exhaustive candidate-vector parity.
  Fused/persistent-axis paired block-sum geometric mean 1.389064x GCC,
  1.299421x Clang; spatial-first 1.194487/1.135972, retained refit
  1.008827/1.050002. Fused ranks first in this aggregate; do not infer
  every-scene superiority. Exact paid query boundary includes candidate
  build/refit, filter, normalize; excludes source physical integration.
  Added optional twelfth arm against stock pinned IPC LBVH on the same
  dense-ID canonical contract; stock-arm compilation and measurement
  must be performed by the final Mac campaign, not credited locally.
  Compressed raw timing CSVs and exact receipts committed with verified Git
  hashes. Cross-platform performance remains workload-specific.
- [x] Replace old partial runner with tools/run_final_campaign.py.
  Fresh default width64, stock IPC + exact CMake build with Boost propagation,
  all registered CTests, independent solver generation and stock replay
  (two-body, three-body fixed, adaptive, prescribed concave decomposition),
  common paid comparison, full-solver timings, exact transaction and long
  audits, previous optimization controls, physical Metal four-arm query and
  guarded Metal producer integration, actual nonconvex geometry outputs,
  terminal tables, SVG/PNG charts, offline dashboard, SHA256 and ZIP.
  No intermediate operator commands. Blocked hardware/dependency, skipped
  tests, incomplete CSV, bad pair/step counts or timeout cannot be promoted
  to pass. If scientific packages missing, isolated local venv only.
  Stock configuration failure still allows native/exact fallback; overall
  completion stays false. No global package install or source checkout edits.
- [x] Actual six Python unit tests pass (process failure, skip, timeout,
  validator rejection, duplicate/ragged/missing receipt, ZIP integrity and
  SHA256). Actual measured data report renders nine figures in PNG and SVG,
  terminal paired ratios and summary, and actual nonconvex 33-frame
  impact/control GIF, shared-volume-scale map and original topology.
  The full new Mac runner itself has not been executed in this Linux VM:
  no Metal device and no stock oneTBB dependency checkout. CMake target
  registration is authored; direct native GCC/Clang apps executed.
  Earlier operator-reported Mac query/stock replay measurements are retained;
  full device-bridge validation and current unified stock IPC timing remain
  new final campaign gates. The concave external solver still uses a prescribed
  decomposition; no arbitrary automatic nonconvex or universal safety claim.

Single command from clean repository root (Mac):
    git switch research/spacetime-contact-atlas && git pull --ff-only && caffeinate -dims python3 tools/run_final_campaign.py

Optional --parallel N, --timeout SECONDS, --output PATH, --no-install.
Default output: output/research-final-<timestamp>; raw data, manifest,
index.html, graphs, geometry, logs, SHA256SUMS, research-evidence.zip.
Nonzero exit means at least one declared gate failed/blocked or reduced
scope. A complete campaign validates only the declared experiments.

## Pass 41B — automatic convex-region contact constraints

- [x] Independent CasADi/IPOPT implicit neo-Hookean FEM, deterministic
  convex-region construction from tetrahedral face connectivity and
  geometry. Greedy hull-volume merge heuristic, all-node hull constraints,
  initial separating axes from face normals and edge-cross-edge axes,
  dynamically optimized unit normals and planes. No prescribed region IDs
  or contact directions. Four cases: U/convex insert, L/concave elbow,
  U/concave elbow, stationary L/elbow control. Region pairings 3x1,
  2x2, 3x2, 2x2; 48/48/60/48 total tetrahedra.
- [x] All 128 implicit steps converged; 33 simultaneous-contact steps,
  stationary control inactive. Minimum J 0.948659975809519,
  maximum momentum error 7.516697131282062e-10 kg m/s,
  maximum stationarity residual 9.394450231373874e-11.
- [x] Actual strict GCC/Clang C++23 direct builds and replay using the
  upstream Tight Inclusion numerical library (local logging shim) at
  zero and .0005 m clearance. 2,291,200 exhaustive CCD checks and 62,245
  exact swept volume pair certificates per compiler; no possible events,
  unresolved certificates, or missing candidates. Fused SAP and rebuilt
  SAP both match exhaustive candidate vectors. Byte-identical replay
  SHA256 29111776e7f218f839d96b2ffdb2068e97253093ad3cf92481a7016c028c43d5.
  The automatic trajectory is regenerated and audited by both stock
  IPC wrapper and direct CCD in the single final Mac campaign.
- [x] Actual automatic motion impact/control GIF and topology/volume
  plots generated, included in dashboard. All six runner Python tests
  pass again after fifth independent family integrated. Earlier local
  CMake subset build passed three targeted tests; complete stock
  dependency configuration and new device bridge remain final Mac gates.
- Limit: convex hull constraints may overconstrain deformed regions;
  region merging is a geometric heuristic, not a general collision
  proof. Exact post-step replay validates these recorded paths only.
  No automatic within-body self-contact response claim for this
  independent integrator. Native solver still covers tested self-contact.
  The paid common benchmark remains its original 22 physical workloads;
  automatic family is separately audited, not included in those timings.
