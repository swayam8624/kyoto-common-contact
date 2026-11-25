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

