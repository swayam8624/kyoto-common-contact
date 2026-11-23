# Kyoto: Spacetime Contact Atlas (research reset)

**Status: PASS 6B — verified 3D neo-Hookean FEM on a curved rigid sphere, independently measured full-face penetration and implemented a conservative whole-triangle time-interval clearance certificate. This is collision detection research, NOT a fully general contact-safe physical solver or an SCA speedup.**

Research question: Can a correctness-preserving locally maintained
swept-contact hierarchy reduce total contact-detection and solver-step
time versus a strong official IPC Toolkit LBVH on moving deformable
3D geometry, especially when contact regions are localized?

This branch starts fresh. Old V4, RBC and CBP algorithms, slide decks,
repetitive demos, output assets, and competing scripts are **absent
from the working tree**, not erased from archival Git history.

Stage-0 implements only a fixed swept-AABB SAP baseline and a separate
quadratic candidate-set oracle. If the primitives do not follow straight
vertex trajectories, the endpoint AABB hull is NOT a certified bound.

Build and validate:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure


## Stage 1 — complete cubic-path envelope and 3D scientific lab

The initial common swept-AABB API remains unchanged. The new
motion_envelopes.hpp module conservatively encloses cubic Bezier
moving triangle vertex paths using control-point convex hulls.
It demonstrates an analytical mid-step triangle coincidence that
unsafe endpoint-only bounding boxes would reject.

THIS IS NOT YET a solver-coupled CCD implementation or a speedup.

Build a verified 121-frame native kinematic study and science figures:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel 6
    ctest --test-dir build --output-on-failure

    ./build/kyoto_motion_export results/motion-lab
    python3 -m pip install numpy matplotlib plotly
    python3 scripts/render_motion_lab.py \
      --data results/motion-lab \
      --output results/motion-lab/visuals
    python3 scripts/export_paraview_motion.py \
      --data results/motion-lab \
      --output results/motion-lab/vtk

Outputs: 3D trajectory certificate, time-position graph, x-y-time
spacetime view, analytical regime map, interactive offline Plotly 3D
replay, 121 time-indexed ParaView meshes, C++ source CSV, manifest.
Every figure reads source geometry rather than guessed physics.

See docs/08_CERTIFIED_MOTION_LAB.md for proof and scope;
docs/03_VISUALS.md for the full gallery;
docs/07_TASK_MATRIX.md for incomplete solver/research work.



## Stage 2 — synchronized interval certificates and selective pair refinement

The C++23 temporal_refinement.hpp supports conservatively bounded
cubic paths split into synchronized slabs using interval de Casteljau.
It tests both a global time-slab sweep and a selective per-candidate
refinement against independent exhaustive pair enumeration.

The local synthetic study has 120 measured method-condition rows, 96
randomized constructive continuous-contact test scenes and two
analytical false-positive families.

**Critical negative:** selective is around 3x faster than global
at 16 slabs in the local 2,048-target example, but the one-slab
baseline remains far faster. This does NOT establish acceleration
of a real collision solver or superiority to official IPC LBVH.

    ./build/kyoto_temporal_experiment results/temporal-study
    python3 -m pip install numpy matplotlib pandas plotly pillow
    python3 scripts/render_temporal_study.py \
      --input results/temporal-study \
      --output results/temporal-study/visuals

The produced scientific atlas includes six figures, measured 3D
cost surfaces, a 3D candidate network and a standalone offline 3D
observatory. Every chart is traceable to the native experiment CSV.
For theory and exact constraints read docs/09_CERTIFIED_TEMPORAL_REFINEMENT.md.

Official IPC Toolkit stable tag for external feature reference: v1.6.1
(19bcab7e1f5a0d2addfcd28513422ff248ea504e). Its vertex-face and
edge-edge candidate API is now tested separately using the new typed mesh contract.



## Stage 3 — mesh-feature semantics, real IPC Toolkit and native 3D

The real new common-feature baseline is
include/kyoto/feature_broadphase.hpp. It independently enumerates
vertex-face and edge-edge pairs from a connected 3D mesh with
stable feature IDs, masks, explicit incidence, cubic swept vertex
boxes and topology epochs. Nonlocal same-object pairs remain valid.

The native correctness test checks 140 randomized moving 3D meshes,
140 shuffled input orderings and adversarial adjacency/topology
conditions against an independent O(E^2+VF) oracle.

The first **actual upstream IPC Toolkit v1.6.1** opt-in integration
built and passed on Ubuntu via CMake FetchContent pinned to
19bcab7e1f5a0d2addfcd28513422ff248ea504e. Across 13
matched-semantic 3D fixtures, official LBVH and our reference each
returned **36,799 normalized candidates, with zero extras or misses**.
This is candidate-set parity, NOT a speed benchmark or CCD proof.

    cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release \
      -DKYOTO_WITH_OFFICIAL_IPC=ON \
      -DIPC_TOOLKIT_WITH_SIMD=OFF
    cmake --build build-ipc --target kyoto_official_ipc_lbvh_tests --parallel 4
    ctest --test-dir build-ipc \
      -R pinned_official_ipc_lbvh_feature_coverage --output-on-failure

The default build stays free of IPC Toolkit's transitive dependencies.
See docs/11_OFFICIAL_IPC_PARITY.md for every actual original receipt.

The new native C++23 mesh scene is a moving triangulated toroidal
shell through a separate breathing spherical cage: 420 vertices,
1,254 edges, 836 faces and 61 source-derived geometry frames.
Original C++ instantaneous feature candidates, native vertex
displacements, surface connectivity and ParaView series are
exported without invented physics.

    ./build/kyoto_feature_scene results/feature-scene
    python3 scripts/render_feature_scene.py \
      --input results/feature-scene \
      --output results/feature-scene/visuals

This produces five scientific figures, a standalone interactive 3D
viewer, and 61 VTK time-series frames. An optional higher-fidelity
VTK studio is in scripts/render_feature_vtk_studio.py:

    python3 -m pip install vtk numpy matplotlib pillow
    python3 scripts/render_feature_vtk_studio.py \
      --input results/feature-scene \
      --output results/vtk-studio

All these 3D objects have *scripted kinematics*, not elastic
contact-response physics. Surface colors are displacement,
and candidate links do not mean true CCD events or impulses.

See docs/10_MESH_FEATURE_BROADPHASE.md,
docs/11_OFFICIAL_IPC_PARITY.md and docs/12_VTK_STUDIO.md.



## Pass 4 — actual upstream 3D linear-motion CCD

The experimental broad phase has advanced beyond candidate-set parity.
tests/ipc_continuous_ccd_tests.cpp exercises the actual pinned
IPC Toolkit vertex-face and edge-edge narrow-phase CCD implementation.
Across 35 deterministic linearly moving 3D mesh cases, it compares
the complete set of true upstream CCD events obtained through official
LBVH candidate filtering against exhaustive independent enumeration
of all topologically admissible feature stencils.

Controlled analytical cases include vertex-face and edge-edge
mid-step collisions, crossings of the infinite triangle plane
outside a finite triangle, noncrossing motion and time-alias
broad-phase false positives. The other 28 seeded random scenes
are checked by full enumeration but do not have independent
analytical event-time labels. Output includes real conservative
CCD time-of-impact and measured stage timings.

    cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release \
      -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
    cmake --build build-ipc --target kyoto_official_ipc_ccd_tests --parallel 4
    ctest --test-dir build-ipc -R pinned_official_ipc_continuous_CCD_coverage \
      --output-on-failure
    ./build-ipc/kyoto_official_ipc_ccd_tests results/ccd-gate
    python3 -m pip install numpy matplotlib
    python3 scripts/render_real_ccd_lab.py \
      --input results/ccd-gate \
      --output results/ccd-gate/visuals

The five new figures are generated from the same C++ / actual upstream
CCD audit. Their source is scientific kinematic geometry, not
simulated contact-response forces. All underlying CCD paths are
LINEAR in each step; cubic Bezier swept envelopes cannot be
handed directly to the linear narrow phase.

See docs/13_REAL_CONTINUOUS_CCD.md for the mathematical and
methodological limitations. Verification: consult the latest
github.com Actions run; a source commit alone is not test evidence.



## Pass 5 — actually integrated 3D hyperelastic physics

A real closed 3D *solid* torus now moves under integrated gravity,
elastic stress and contact impulses. It is NOT an animated or
prescribed-deformation surface. The C++23 core is
include/kyoto/solid_dynamics.hpp: 180 mass-carrying 3D vertices,
480 volumetric tetrahedra, actual compressible neo-Hookean W(F),
analytical first Piola forces, true Cauchy von Mises stress and
a limited analytical z>=0 rigid-plane impulse response.

Verification: independent 900-step native physics tests on macOS
and Ubuntu in Debug and Release, checking FEM energy-force finite
difference, invariance, mass conservation, free-fall COM,
positive Jacobians, no floor penetration and actual contact.
Complete 900-step science and visualization CI:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45072204086

Actual native receipt: 0.675 s, 201.845 kg body, 0.7403 minimum
det(F), 7.018 kPa peak von Mises and 10,380 vertex-step floor
impulse records. Those records are NOT distinct collision episodes.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/kyoto_solid_simulation results/solid-fem
    python3 -m pip install numpy matplotlib pandas plotly pillow
    python3 scripts/render_solid_fem.py \
      --input results/solid-fem \
      --output results/solid-fem/visuals

Outputs: six *real physical* plots, source-backed interactive
3D von Mises viewer, 181 volumetric tetrahedral ParaView frames,
contact location events, energy and material fields. Optional
cinematic high-fidelity VTK studio + 31-frame native-motion GIF:

    python3 -m pip install vtk matplotlib pillow numpy
    python3 scripts/render_solid_vtk_studio.py \
      --input results/solid-fem \
      --output results/solid-fem/studio --gif

Independent native VTK CI:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45072461124

The official IPC Toolkit reviewer in
tests/ipc_solid_trajectory_audit.cpp is a separate opt-in
cross-object/self-contact survey on ACTUAL accepted integrator
steps. It must report unresolved self events rather than quietly
claiming self-contact-safe dynamics.

**Limits:** the solver is explicit-force, the ground is an
analytical infinite plane with lumped impulses, there is no
general mesh-mesh IPC constraint response, no two-solver
replication, no demonstrated SCA speedup or novelty.

Read docs/14_FIRST_VOLUMETRIC_FEM.md for the full derivation,
numerical assumptions, source manifest and open science gates.



## Pass 6 / 6B — curved rigid contact, truthful negative evidence,
## and a mathematically conservative full-triangle sweep certificate

Our native C++23 **real 3D hyperelastic toroidal solid** now falls
onto a curved analytical rigid sphere. It integrates all 180 vertices,
480 tetrahedra and physical material stress, velocity and gravity
over 900 true steps (181 volume snapshots). It records swept
vertex-sphere CCD, contact impulses and real neo-Hookean energy.

Critically, an independent finite-triangle closest-point oracle
checks **all 901 accepted states**, including the initial state:

- **0** accepted states with a vertex inside the sphere;
- **458/901** accepted states with penetrating *triangle interiors*;
- **21.0999 millimetres** maximum measured triangle penetration;
- first triangle overlap t=0.2265 s, before first vertex
  contact impulse t=0.2295 s.

This negative observation proves **vertex-safe is NOT face-safe**.
It is independently audited and preserved in the signed native CSV
rather than patched out of the visualization.

Verified cross-platform CI:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45133327667
Native evidence artifact:
kyoto-stage6-real-curved-FEM-and-surface-safety-audit
(artifact ID 14798149536).

The science renderer and 3D volume export are reproducible:

    ./build/kyoto_curved_fem_export results/curved-fem
    python3 scripts/render_curved_fem.py \
      --input results/curved-fem \
      --output results/curved-fem/visuals

Outputs are six source-backed figures, a standalone rotatable
3D solver playback, 181 true tetrahedral ParaView frames, a
real normal-impulse trace, and the 901-step vertex-vs-face audit.

**New Pass 6B source:** include/kyoto/triangle_sphere_sweep.hpp
implements a conservative *moving triangle/sphere* separation
certificate based on the Lipschitz inequality:

    |distance(T(t),c)-distance(T(s),c)|
        <= max_i ||p_i(t)-p_i(s)||.

A synchronized time interval is certified separated when its
midpoint triangle distance minus half its maximum vertex motion
is greater than sphere radius. It bisects uncertain intervals
and returns the first unverified interval as a safe time-prefix
bound. The independent test uses analytic face tunnelling,
640 randomized 3D swept triangles with 128,640 path samples,
and the FIRST real FEM triangle-penetration transition.

The certificate is **not yet used as a physical contact-response
solver**; its exact-arithmetic theorem does not substitute for
directed-rounded floating-point verification. The immediate next
scientific gate is to integrate globally synchronized full-face
safe advancement and physically correct contact impulses,
then audit every accepted step and compare official IPC CCD.

See:
docs/16_CURVED_SPHERE_FEM.md and
docs/17_TRIANGLE_SWEEP_CERTIFICATE.md.


Read the docs in numbered order:
- docs/01_PROBLEM.md
- docs/02_STRATEGY.md
- docs/03_VISUALS.md
- docs/04_RESET.md
- docs/05_PRIOR_ART.md
- docs/06_METHOD_BLUEPRINT.md
- docs/07_TASK_MATRIX.md
- docs/08_CERTIFIED_MOTION_LAB.md
- docs/09_CERTIFIED_TEMPORAL_REFINEMENT.md
- docs/10_MESH_FEATURE_BROADPHASE.md
- docs/11_OFFICIAL_IPC_PARITY.md
- docs/12_VTK_STUDIO.md
- docs/13_REAL_CONTINUOUS_CCD.md
- docs/14_FIRST_VOLUMETRIC_FEM.md
- docs/15_IPC_SOLVER_MOTION_AUDIT.md
- docs/16_CURVED_SPHERE_FEM.md
- docs/17_TRIANGLE_SWEEP_CERTIFICATE.md
- AGENTS.md

The NEXT gate is conservative whole-face safe advancement plus validated
rigid/deformable mesh impulses and self-contact, before any matched
full-paid SCA-vs-official IPC LBVH performance claim.
Do not substitute candidate parity for actual continuous collision proof.
