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
- [ ] Fully independent *general VF/EE mesh-mesh* continuous CCD mathematical oracle and numerical robustness audit (sphere-only Pass 9 is not general IPC).

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
- [ ] General triangle-mesh rigid/deformable response and self-contact with official IPC CCD, beyond a smooth static analytic sphere.

## Physics scenes and adapters

- [x] First actual freely deforming 3D volumetric hyperelastic solid against flat plane (not yet a self-contact-rich or IPC-constrained solver).
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
- [ ] Strain tensor contour/isosurface and general mesh-collision impulses from a validated solver.
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
