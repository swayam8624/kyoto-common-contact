# Kyoto: Spacetime Contact Atlas (research reset)

**Status: PASS 10 — independent outward-enclosed IEEE-754 swept-triangle/sphere separation with exact-dyadic time interval enclosures derived from ORIGINAL vertex endpoints, and a full 3D FEM gravity/elastic/damping/contact discrete momentum ledger. All 288,000 routine and 1,600 fast accepted moving triangle intervals were strictly verified with 0 undecided in the two tested trajectories; every one of 905 source FEM leaves passed the full P/L accounting gate. Five cross-platform/visual CI jobs verified. General IPC mesh-mesh CCD, formal hardware/compiler numerical proof, calibrated physical energy fidelity, self-contact and the central SCA speedup hypothesis remain OPEN.**

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



## Pass 7A — real full-face-safe nonplanar FEM reference

**Verified original native evidence:**
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45138998647

The core in include/kyoto/whole_face_contact_dynamics.hpp
implements two distinct safeguards on the SAME physically integrated
180-vertex, 480-tetrahedron, 320-face neo-Hookean torus:

1. A barycentric mass-weighted contact impulse applied at a true
   finite-triangle nearest point, with effective contact inverse
   mass sum(w_i^2/m_i) and bounded tangential friction.
2. The strict whole-triangle Lipschitz conservative time certificate
   on EVERY proposed moving triangle. If any possible sphere
   contact cannot be certified separated, ONE minimum prefix is
   applied to ALL 3D vertices before committing a timestep.

The earlier vertex-only solver remains the negative control and
is never rewritten. Two independently run 900-step FEM exporters
with identical reference topology, gravity and sphere give:

| Measured condition | Original vertex-only | Whole-face response |
| --- | ---: | ---: |
| States with penetrating triangular faces | **458/901** | **0/901** |
| Minimum finite-triangle clearance | **-21.0999 mm** | **+1.1277 mm** |
| States with vertices inside sphere | 0 | 0 |
| Physics timesteps | 900 | 900 |

The new run includes 12,629 nonzero vertex-step impulse
contributions from 10,049 face-level Gauss–Seidel impulse
updates, and computes actual stress and energy. Its
peak Cauchy von Mises is about 22.24 kPa; min det(F) 0.7649.

**Causal caveat:** normal 900-step reference run clipped **zero**
positions (global alpha=1 at every timestep); early face-aware
velocity impulses maintained clearance. An independent extreme
900 m/s one-step tunnelling case specifically tests that the
global whole-face certificate DOES activate (alpha<1) and
keeps the connected triangle surface safe.

This is finite tested geometry, not universal machine-interval
correctness. Global clipping, when invoked, can alter physical
energy and displacement during a timestep because the remaining
fraction is not re-integrated. The sequential contact response
is not globally implicit. No arbitrary moving rigid meshes,
self-contact or general IPC barrier solver yet.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

    ./build/kyoto_whole_face_fem_export results/whole-face-fem
    python3 scripts/render_whole_face_fem.py \
      --input results/whole-face-fem \
      --output results/whole-face-fem/visuals

    ./build/kyoto_curved_fem_export results/curved-fem
    python3 scripts/compare_curved_contact_solvers.py \
      --old results/curved-fem \
      --new results/whole-face-fem \
      --output results/whole-face-comparison

The original CI archives the independent trace sources,
901-state comparisons, seven scientific charts, rotatable
3D physically integrated mesh, 181 ParaView tetrahedral volume
states, and a separate true-energy/trajectory-difference audit.

The optional scripts/render_whole_face_vtk_studio.py generates
source-backed cinematic VTK hero frames, a real tetrahedral
cutaway and a short actual-solver GIF, with its own
whole-face-vtk-studio.yml reproducibility job.

See docs/18_SYNCHRONIZED_WHOLE_FACE_CONTACT.md.



## Pass 8 — actually re-integrated elapsed physical time

The Pass-7 whole-triangle-safe solver has been extended with
a transactional, adaptive time-replay integrator:
include/kyoto/time_consistent_contact.hpp.

An earlier solver could accept only fraction alpha<1 of a trial
displacement while keeping the velocity predicted for the full dt
and charging the ENTIRE dt to the physical clock. This was a
scientifically important defect, even though the usual 900-step
impact had alpha=1 at every step.

**Pass 8 refuses to commit those clipped trials.** Instead it
splits the physical timestep in two and computes fresh stresses,
accelerations, contact impulses and conservative full-triangle
certificates for EACH half in sequence. Only accepted FULL
substeps contribute to elapsed time; their sum must equal
the original requested dt.

A trial that would invert a tetrahedron is rejected and retried
at smaller dt; no invalid state is committed. Exhausting
the configured temporal budget fails loudly and rolls back
the ENTIRE original macroscopic step, preserving external
raw receipts as well.

The native C++ tests cover:
- 40 free-flight steps matching the previous uncut integrator;
- the full 900-step real 3D hyperelastic impact, with no
  penetrating entire surface triangles and correct .675 s clock;
- a specially constructed 50 m/s close-sphere impact with an
  initial full-triangle clearance of 15 mm, requiring genuine
  physical subdivision across the *complete* requested .75 ms;
- transactional rollback on an intentionally insufficient budget.

The original unphysical 900 m/s impact exhibited inverted
element proposals and exhausted the temporal limit; it remains
a documented negative extreme-stability case, NOT silently
reclassified as solved.

New native evidence app:
apps/time_consistent_experiment.cpp produces 901 routine state
receipts, every accepted physical fraction at the fast
collision, original vs adaptive vs 256-finer-step SAME-model
results, and source 3D stress/vertex states.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/kyoto_time_consistent_experiment results/time-consistent-fem

    python3 -m pip install numpy pandas matplotlib pillow
    python3 scripts/render_time_consistent_lab.py \
      --input results/time-consistent-fem \
      --output results/time-consistent-fem/visuals

The five scientific plots expose actual time accounting,
event contact impulses, independent full-triangle clearance,
routine 900-step elastic energies and the original native
3D mesh geometry at four different numerical outcomes.

A 256-small-step result is a *finer version of the SAME
numerical solver*, not an independent physical ground truth.
The sequential contact law and finite clearance skin still
limit contact fidelity; the exact-arithmetic Lipschitz argument
is not yet certified under directed-rounded binary64.

See docs/19_TIME_CONSISTENT_REPLAY.md for proofs, sources,
negative outcomes and the exact remaining acceptance gates.



## Pass 9 — independent 3D continuous contact and impulse audit

We now have two distinct mathematical arguments for separating
**entire moving triangular faces** from the curved rigid sphere:

- Original solver: midpoint finite-triangle distance plus a
  Lipschitz motion bound with recursively subdivided time.
- Independent research auditor: for linearly moving triangle
  vertices, the entire swept surface lies inside the convex
  hull of their **six start/end points**. A strict separating
  plane placing all six points on one side of the rigid sphere
  certifies continuous clearance without using the solver's
  original distance routine.

The second method proposed planes from coordinate directions,
endpoint directions, triangle normals, and independent
convex-hull support search. Each potential certificate is
actually checked against all six 3D endpoints. Failure to
find a separating plane remains UNRESOLVED, not "safe."

Measured **original native FEM paths** (verified in a 5-job
Ubuntu/macOS Debug/Release + scientific-visual CI run):

| Independent continuous triangle/sphere evidence | Value |
| --- | ---: |
| Routine integrated physical time-steps | 900 |
| Routine physical face paths independently certified | **288,000 / 288,000** |
| Fast-impact accepted substeps | 5 |
| Fast physical face paths independently certified | **1,600 / 1,600** |
| Unresolved intervals in either experiment | **0** |
| Actual routine independent separating-plane tests | **4,916,586** |
| Maximum disagreement with separate Voronoi triangle distance oracle | **2.22e-16 m** |

The original actual 3D contact law also passes an independently
derived physics audit over **1,200 randomized rotated and
translated non-axis-aligned contacts with unequal masses**.
Maximum residuals: local impulse kinetic-energy identity
6.37e-12 joules, linear impulse momentum 1.71e-13 N*s,
and angular impulse momentum 9.37e-13 N*m*s.

The precise local impulse theorem is:

    k = sum(w_i*w_i/m_i)
    delta_P = J
    delta_L = q cross J
    delta_K = u dot J + (k/2)*||J||^2

with u=sum(w_i v_i) and contact point q=sum(w_i x_i).
For 0<=restitution<=1 and bounded tangential friction, one
such local frozen-position impulse does not create kinetic
energy in exact arithmetic. This **does not** establish
total energy conservation of the full damped, gravitating
explicit neo-Hookean FEM solver.

Reproduce the separate oracle and measured data:

    ./build/kyoto_independent_contact_tests results/pass9-independent
    ./build/kyoto_impulse_balance_tests results/pass9-independent
    python3 scripts/render_pass9_independent_audit.py \
       --input results/pass9-independent \
       --physics results/time-consistent-fem \
       --output results/pass9-independent/visuals

The resulting research archive contains original independent
900-step face-interval receipts, all five fast physical leaves,
all 1,600 fast face-level source proof statuses, all 1,200
separate impulse-law receipts, and seven original academic
figures including a 3D physical FEM stress/clearance atlas.

Verified original CI:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45144892694

See docs/20_INDEPENDENT_CONTINUOUS_CONTACT_AUDIT.md for
theorems, mathematical derivations, experiments and limitations.

**Do not extrapolate "zero unresolved in the two tested
trajectories" to machine-certified CCD over all possible
floating-point inputs.** The real-arithmetic separation theorem
is valid, but universal IEEE directed-rounded verification,
arbitrary moving triangle meshes and self-contact are still open.



## Pass 10 — outward floating-point bounds and whole-FEM physics

**The geometry breakthrough is a stronger NUMERICAL CHECK, not
a new collision solver.** Rather than relying on nominal
double-precision dot products or arbitrary relative epsilon,
the independent Pass-9 six-endpoint separating-plane test
now evaluates each subtraction, multiplication, addition,
square and square root using conservative outward-enclosed
binary64 intervals (under explicitly tested IEEE/FE_TONEAREST
assumptions).

A successful plane must have a strictly positive LOWER
bound against the UPPER bound of sphere support in all
six swept endpoint intervals.

For temporal subdivision, the C++ verifier reconstructs
position INTERVALS at exact dyadic fractions from the
ORIGINAL vertices, not rounded midpoint positions that
could create unintended gaps in the path.

Source: include/kyoto/rounded_prism_certificate.hpp
and tests/rounded_prism_certificate_tests.cpp.

**Original source-backed FEM measurements:**

| Outward interval continuous proof | Routine 900-step FEM | Fast 50m/s impact |
| --- | ---: | ---: |
| Full moving surface-face intervals | 288,000 | 1,600 |
| Strictly proven separated under stated IEEE model | **288,000** | **1,600** |
| Unresolved | **0** | **0** |
| Candidate separating planes paid | 1,939,204 | 11,874 |
| Outward-enclosed endpoint dot checks paid | 3,536,502 | 21,495 |
| Smallest positive lower plane margin | 2.069 micrometres | 98.242 micrometres |

This is true source-based per-operation proof work, **not**
a wall-clock SCA versus IPC LBVH benchmark or general
triangle/triangle continuous collision detector.

**Entire physical 3D FEM momentum accounting:** the source
contact law now exposes its actual 3D vector impulses and
their torques. For every accepted physical substep, an
independent full-volume ledger re-evaluates all elastic
forces, gravity and damping; measures before/after nodal
momentum, and includes the discrete angular transport
term resulting from changing vertex positions:

    Pnew = damp*(Pold + dt*sum(Felastic + m*g)) + Jcontact

    Lnew = damp*(Lold + dt*sum(xold cross F))
           + sum(xold cross J_contact)
           + sum((xnew-xold) cross (m*vnew))

All 900 routine leaves plus five fast physical substeps
passed. Maximum relative residuals in the routine case:
1.84e-15 linear momentum and 1.40e-13 angular momentum;
fast case: 1.75e-15 and 1.10e-12.

We also measure every real mechanical energy delta.
The full 900-step scene lost **459.624 J** of combined
kinetic, elastic and gravitational energy; the separate
short fast collision lost **14,545.237 J** under its
reference model. This is explicitly NOT a validated
physical energy budget, because damping, friction and
time discretization contribute. It reinforces the need
for external solver comparisons and a convergence study.

Native files:

    include/kyoto/rounded_prism_certificate.hpp
    include/kyoto/whole_face_contact_dynamics.hpp
    include/kyoto/time_consistent_contact.hpp
    tests/rounded_prism_certificate_tests.cpp
    tests/whole_fem_balance_tests.cpp
    scripts/render_pass10_numerical_audit.py

Reproduce the new source-based laboratory:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/kyoto_time_consistent_experiment results/time-consistent-fem
    ./build/kyoto_rounded_prism_tests results/pass10-rounded
    ./build/kyoto_whole_fem_balance_tests results/pass10-rounded
    python3 scripts/render_pass10_numerical_audit.py \
       --input results/pass10-rounded \
       --physics results/time-consistent-fem \
       --output results/pass10-rounded/visuals

GitHub CI archives all original trace tables and seven measured
scientific charts, including the SAME native 3D FEM geometry
colored by computed stress and strict lower proof margin, plus
a fully offline interactive rotatable 3D viewer that switches
between both native scalar fields.

Verified cross-platform run:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45153954633

Complete mathematical derivations and numerical
assumptions: docs/21_ROUNDED_PRISM_AND_WHOLE_FEM_BALANCE.md.

A proof under declared IEEE assumptions is not
universal hardware/compiler/exact-predicate
certification. Self-contact, moving triangle-mesh
rigid obstacles, external physical calibration,
fully implicit contact and a demonstrated SCA
performance improvement remain unmet gates.


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
- docs/18_SYNCHRONIZED_WHOLE_FACE_CONTACT.md
- docs/19_TIME_CONSISTENT_REPLAY.md
- docs/20_INDEPENDENT_CONTINUOUS_CONTACT_AUDIT.md
- docs/21_ROUNDED_PRISM_AND_WHOLE_FEM_BALANCE.md
- AGENTS.md

The NEXT gate is general triangle/triangle rigid/deformable contact and
self-contact with robust feature-level CCD, independent physical energy/
convergence validation, and a truly maintained whole-cost SCA hierarchy
versus the pinned official IPC Toolkit LBVH on held-out 3D solver paths.
Do not substitute candidate parity for actual continuous collision proof.
