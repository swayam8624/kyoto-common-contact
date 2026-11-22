# Pass 3 — Common broad phase for moving 3D meshes

**Status:** native C++23 reference implementation and kinematic 3D visualization.
**NOT:** an official IPC Toolkit adapter, a full CCD engine, physically
integrated deformable solver, or an algorithmically novel broad phase.

## 1. Why triangle-triangle boxes are not an IPC-equivalent comparison

A collision solver ultimately needs geometric feature pairs.
For a triangle surface, the important candidate families include
**vertex-face (VF)** and **edge-edge (EE)**. A number of public
simulators support more combinations (face-face, edge-face etc.).
The exact set depends on the consuming solver and its collision model.

An old triangle-triangle swept-AABB pair count cannot be compared
directly with an official library's vertex-face or edge-edge
candidate count. One triangle-triangle pair may expand to many
underlying feature pairs, or may be filtered by topology and
dimensionality. A different count does not imply faster/more
precise geometry until the semantics have been matched.

This pass introduces a common feature contract with stable typed
IDs and an explicit topology epoch. Only two feature families are
supported yet: VF and EE. It intentionally exposes that limitation.

## 2. Mathematical candidate specification

Let V be vertices, E be edges, F be triangular faces. Each vertex
v(t) follows its declared 3D cubic Bezier path. Its conservative
control-point AABB with reach h is S_v.

The swept envelope for an edge e=(a,b) is

    S_e = hull(S_a, S_b).

The swept envelope for a face f=(a,b,c) is

    S_f = hull(S_a, S_b, S_c).

For each time t, the moving segment and triangle are convex
combinations of their simultaneous moving vertices. Therefore,
in exact arithmetic, these boxes enclose the whole edge or face
at every t in [0,1], assuming all vertex trajectories obey the
supplied path contracts.

Define a binary mask predicate A(i,j) and *incidence* predicates
I_VF(v,f): v in vertices(f)
I_EE(e1,e2): edges share an endpoint.

Our current candidate sets:

    B_VF = {(v,f): v in V, f in F,
            not I_VF(v,f), A(v,f), S_v overlaps S_f }

    B_EE = {(e1,e2): e1<e2 in E,
            not I_EE(e1,e2), A(e1,e2),
            S_e1 overlaps S_e2}.

The output is a sorted unique vector of tagged pairs
(kind, first_id, second_id). Feature IDs are stable within a
topology epoch, but the reference path rebuilds from inputs on
every query. The future incremental hierarchy must use the
epoch to invalidate structural assumptions.

**Nonlocal self-contact is allowed:** two features belonging to the
same object are not automatically excluded when they are not
incident. More aggressive one-ring or rest-shape filters are
solver-dependent, require an explicit documented policy, and are
not silently applied here.

**Candidates are NOT contact events.** A broad phase does not
enforce collision response, compute penetration, or decide the
true continuous time-of-impact.

## 3. Two independently structured algorithms

The exhaustive oracle tests O(|V||F|+|E|^2) possible pairs.
It is intentionally simple and is the test reference for small N.

The fixed sweep-and-prune reference separately sorts X start
coordinates, discards intervals whose max X is strictly behind the
current min X, applies identical masks/incident filtering and
checks complete AABB overlap in XYZ. Inclusive touching is
preserved. It uses two active interval groups for cross-type VF
and one active group for EE.

Every candidate is normalized by its tagged stable feature IDs.
The two algorithms are implemented separately so a coding error
in sweep order can be detected by equality tests.

## 4. Correctness and limitations

Automated native C++ tests cover:
- a swept vertex moving across the plane of a triangle halfway
  through a cubic path, including a nonlocal same-object VF pair;
- incident VF and shared-vertex EE suppression;
- valid cross-object overlapping EE candidates;
- bidirectional mask exclusions and topology-epoch change;
- duplicate IDs, missing endpoints, degenerate faces, NaNs;
- 140 seeded random moving 3D mesh populations with 28 vertices,
  26 edges and 20 faces each, plus 140 independently shuffled
  input orderings; exhaustive vs SAP parity in each.

These tests establish finite tested parity for the stated
feature-level AABB contract. They are NOT general CCD proofs.
No numerical guarantee is asserted for arbitrary nonlinear
physics solvers, floating-point modes, or time-varying topology
during a query. The feature envelopes use nextafter expansion
in IEEE-style doubles but are not machine-certified intervals.

## 5. Native 3D geometry fixture

The C++ generator creates a toroidal triangulated shell whose
vertices travel along cubic paths while a triangulated geodesic
sphere/cage gently changes radius. The surfaces are closed and
distinct, without the old anchored cloth and ball aesthetic.

The source mesh has:
- 420 vertices total (198 torus, 222 geodesic cage);
- 836 triangles total (396 torus, 440 cage);
- 1,254 deduplicated topological edges;
- 61 exported geometrically evaluated states at t=0..1.

These objects are scripted *kinematic geometry*, not elastic
mass-spring or finite-element simulation. Native feature-level
SAP calculates each frame's instantaneous AABB candidates.
Frames 0, 30 and 60 are checked against the independently
implemented exhaustive oracle. The exporter records every frame's
typed VF/EE candidates and the vertex displacement from t=0.

No candidate link may be labeled a confirmed impact, force,
contact impulse or CCD time-of-impact without independent
narrow-phase geometry testing.

## 6. Scientific visual atlas

The Python renderer reads *only* source-generated CSVs:
- mesh_states.csv: xyz, object IDs, displacement scalar, frame
- faces.csv and edges.csv: stable topological connectivity
- typed_candidates.csv: per-frame tagged VF and EE candidate IDs
- frame_metrics.csv: per-frame candidate counts and AABB tests
- scene_manifest.json: scene provenance and semantic limitations

And generates:
1. Matched four-camera 3D mesh montage (t=0,.333,.667,1).
2. Surface scalar displacement visualization with fixed legend.
3. Embedded 3D VF/EE candidate graph from actual feature centroids.
4. Timeline of per-frame candidate counts separated by kind.
5. Spatial density field of sampled AABB candidate-link midpoints.
6. Standalone offline interactive 3D replay with geometry scrub.
7. 61 actual ParaView VTK frames and a time-series .pvd.
8. Full scientific figure atlas PNG.

Displacement is a *kinematic geometric scalar*, not computed stress
or strain, and density is candidate-link frequency, not pressure.
The graph shows stratified sampled links, with the full counts
provided in the source table.

## 7. Reproduction (macOS/Linux)

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

    ./build/kyoto_feature_scene results/feature-scene
    python3 -m pip install numpy matplotlib plotly pandas pillow
    python3 scripts/render_feature_scene.py \
      --input results/feature-scene \
      --output results/feature-scene/visuals

Open the standalone interactive HTML or
results/feature-scene/visuals/vtk/mesh_series.pvd in ParaView.
The workflow checks and archives the native evidence artifacts.

## 8. IPC adapter: exact external contract next

Official public IPC Toolkit C++ release **v1.6.1** remains pinned:
commit 19bcab7e1f5a0d2addfcd28513422ff248ea504e.

IPC's BroadPhase/LBVH API accepts vertex positions and edge/face
connectivity or explicit vertex AABBs, and emits edge-edge and
face-vertex candidates with additional solver-specific filters.
An official reference comparison MUST match:
- identical vertex/edge/face IDs and original topology;
- identical inflation radius (mind both sides' conventions);
- identical linear endpoint or externally supplied path envelopes;
- identical incidence/adjacency and masks, or report distinctions;
- unique stable candidate normalization;
- all paid feature box extraction, maintenance, query and allocation;
- valid independent continuous CCD event truth;
- original package version and compiler/compiler flags.

We do NOT claim a parity test against the official IPC Toolkit yet.
The next research step should create an opt-in real pinned IPC
build/adapter, obtain actual reference output, then integrate
real contact-rich solvers. A shim pretending to be IPC is prohibited.

## 9. Research meaning

A genuinely publishable new result still needs a correctness-preserving,
profiler-motivated locally maintained hierarchy that improves over
**real official LBVH** on independent, realistic, contact-rich scenes.
The typed baseline and attractive 3D fixture are enabling
infrastructure, not novelty.
