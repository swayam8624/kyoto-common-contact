# Pass 21 — Multiple truly integrated deformable FEM sources (uncoupled)

## Experimental purpose

Before implementing coupled deformable–deformable collision response, the
Kyoto Spacetime Contact Atlas must prove it can **index and compare
separate physically integrated 3D objects** in a unified source mesh, with
independent source IDs, solver-generated trajectories, and the *same*
candidate rules as official IPC Toolkit LBVH.

This is a **multi-object broadphase scaling test**, not a complete
two-deformable collision solver.

The original reference uses two independent volumetric compressible
neo-Hookean FEM bodies, with different finite-element resolutions,
material Young's moduli, and initial velocities. Each body's physical
timestep is genuinely integrated by the original finite rigid-triangle
contact solver against a shared static connected mesh.

## New physically generated source

- Body A: toroidal 12×3×3 volumetric mesh; 288 tetrahedra;
  nominal Young's modulus 18,000 Pa; initial displacement −1.3 m x.
- Body B: toroidal 16×3×3 volumetric mesh; 384 tetrahedra;
  nominal Young's modulus 26,000 Pa; initial displacement +1.3 m x.
- Shared rigid triangular obstacle: original corrugated finite patch.
- Both bodies have independent x/y drifts, gravity, material stiffness,
  deformation, elastic force and mesh-contact response.
- The cross-body physical response is **not** implemented. Therefore,
  every actual accepted timestep is required to consist of ONE
  time-integrated leaf for each body and to have a strictly positive,
  outward-rounded swept-x coordinate separation margin of 2 mm.
- The independent sampled finite face/face closest-distance audit
  must additionally stay above 1 mm.
- Entire 360 macro-step source is original solver state; benchmark
  records a deterministic subset according to `sample_stride`.
- All geometry, topology, source flags and benchmark timings are
  written only by the native C++ executable and downstream source CSV
  parser. No plotting tool synthesizes original solver states.

The x-axis separation certificate is deliberately **sufficient, not
necessary**. Rejected trajectories may actually be safe; the
fixture must fail closed rather than incorrectly simulate unsupported
mutual contact.

## Global feature index contract

The shared `features::Mesh` contains all original moving vertices,
faces and unique undirected edges from each body, plus the original
static patch. Global IDs are exactly source vertices with contiguous
body offsets; labels are body A=0, body B=1 and rigid patch=2.

All visible faces and edges are renumbered deterministically. No two
bodies share a vertex ID. Therefore incidental adjacency suppression
cannot silently drop legitimate cross-object candidates.

The new `fem::uncoupled_two_body_feature_snapshot` checks that
the original source body node/tet counts and face/edge connectivity did
not change between before and after. It rejects invalid skin, empty
source objects, broken IDs and failed full source feature validation.

Every motion interval contains the actual beginning/ending physical
positions of each vertex and the unchanged rigid patch positions.
Interpolation is the same explicit straight-line source assumption as
the existing reference feature benchmark.

## Correctness and performance comparisons

`tests/uncoupled_multibody_fem_tests.cpp` must match all independent
candidate sets on real source steps:

- exhaustive O(V×F + E²) source reference;
- fresh rebuilt sweep-and-prune;
- persistent full-refit BVH;
- persistent selective-motion BVH.

An adversarial negative control moves the second body into the first
one's swept x-domain and checks that the **uncoupled** simulator's
separation witness refuses to certify it.

The official pinned IPC comparison runs `scene_count=4`:

```bash
cmake -S . -B build-ipc -DCMAKE_BUILD_TYPE=Release \
  -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
cmake --build build-ipc --parallel 4 \
  --target kyoto_pass15_official_ipc_fem_benchmark \
           kyoto_uncoupled_multibody_fem_tests
ctest --test-dir build-ipc --output-on-failure \
  -R 'real_two_body_FEM_uncoupled_shared_feature_oracle'
./build-ipc/kyoto_pass15_official_ipc_fem_benchmark \
  results/pass21-multiple-real-bodies 12 4 4
python3 scripts/report_pass15_official_ipc.py \
  --input results/pass21-multiple-real-bodies/official_ipc_fem_paired.csv \
  --output results/pass21-multiple-real-bodies/science
```

`scene_count=1,2,3` preserve existing experiments. The fourth
physical scene has `scene_id=4` and
`regime=uncoupled_multibody`; the third one stays the original
single-body 20 m/s adaptive fast-impact source, not a second solver.

Both full paid official IPC and equally cached-input IPC are measured
independently from our selected and full-refit hierarchies. Every
candidate output must match or cover the independent reference
according to its precise published source contract.

## What this does NOT establish

- True deformable–deformable **collision response**, forces, impulses,
  coupled momentum/energy exchange, or contact-time integration.
- General deformable self-contact or articulated/moving rigid contact.
- Full solver-step speedup: source FEM integration happens outside
  all broadphase candidate timers.
- Contact event parity for arbitrary *moving–moving* triangle CCD.
- Generality beyond these original physical bodies, fixed materials
  and the finite static obstacle.
- Algorithmic novelty: incremental BVHs and front tracking are prior
  published ideas. A novel selective maintenance/scheduling result
  still requires a strong literature matrix, independent physical
  solvers and fair matched comparisons.

The **next** physical-model gate is to add a genuinely coupled,
two-deformable contact-response solver with equal-and-opposite
mass-weighted impulses, explicit moving/moving swept CCD rejection,
transactional accepted adaptive leaves, and independently checked
momentum and contact-energy audits. Until that implementation is
tested, the present trial must retain its `uncoupled` label.
