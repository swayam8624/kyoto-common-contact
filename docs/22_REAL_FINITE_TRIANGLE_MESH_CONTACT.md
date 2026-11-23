# Pass 11 — From a mathematically smooth sphere to real finite triangular rigid geometry

The central Kyoto thesis remains a *correct, incrementally maintained,
fully paid common swept-contact broadphase* for arbitrarily deforming
3D triangle meshes. Passes 5–10 worked with a real **deforming**
neo-Hookean solid, but its physical obstacle was a smooth analytical
sphere or a plane. That was an important limitation: if the collider
has finite triangles, **edges and vertices are part of the real
geometric contact**, and a sphere-specific nearest-face direction
is no longer an adequate contact law.

Pass 11 changes the COLLIDER GEOMETRY, not just the image.
The obstacle is a connected nonplanar original static triangle mesh
of **49 original rigid vertices and 72 indexed finite faces**.
The deformable body remains the actual freely moving toroidal
**180-node, 480-tetrahedron, 320-surface-triangle** neo-Hookean body.

Every tested collision calculation reads real finite triangle
coordinates and returns feature locations and barycentric weights.
The response uses those positions to update the body dynamics.
No code silently asks the old sphere for a reaction impulse.
No rigid-mesh physics is synthesized from Python.

A separate sphere-only control uses the SAME rigid patch for
independent geometric AUDITING, but intentionally does not
react to it. That is an important correctness counterexample,
not a fair runtime comparison between equivalent solvers.

## 1. Why endpoints and VF/EE distances alone are insufficient

Picture a thin wooden triangle suspended above a sheet of glass
made of another finite triangle. At the next time step the
wooden triangle is under the glass. Both BEGINNING and ENDING
separations may be positive. But the wood passed through the
glass between those timestamps.

Static geometry is already subtle. A finite triangle pair can
be nearest at any of these feature pairs:

- one of three deformable vertices against a rigid triangular face;
- one of three rigid vertices against a deformable triangular face;
- any of nine edge/edge feature combinations;
- an edge passing through the INTERIOR of the opposite face.

The last case is easy to miss. Imagine holding a slender pencil
through the center of a triangular window. Neither pencil end
needs lie on the window, and its length can pass straight
through the interior without touching the window's perimeter.
If all you test is endpoints and distance from the pencil to
the window's EDGES, you may miss that exact piercing.

The C++ implementation therefore uses *both*
**seven-region Voronoi closest-point classification** for
finite vertex/triangle distances, **nine true segment/segment
closest features**, AND **two-sided segment/triangle piercing**.

When segment piercing is found, the collision point carries
the barycentric contact weights on BOTH original triangles.
That is needed later for correct mass-weighted deformable
contact impulses.

Source:

    include/kyoto/triangle_mesh_contact.hpp

Key functions:

    triangle_triangle_closest(a, b)
    closest_segments(p, q, r, s)
    segment_triangle_piercing(p, q, rigid, t, w)

The nearest-feature implementation is currently a finite,
ordinary-binary64 geometric kernel. It is **not** yet a
formally machine-directed-rounded exact predicate and
does NOT establish robust general IPC Toolkit CCD.

## 2. The continuous guarantee and what it actually means

The three deformable face vertices move linearly during a
single accepted physical substep:

    p_i(s) = (1-s)*a_i + s*b_i, 0<=s<=1, i=0,1,2.

Let T(s) be the resulting finite moving triangle, and S a
STATIC finite rigid triangle. Both are compact sets.

For any two times s,t we may transfer the same barycentric
weights w_i from T(s) to T(t). The distance between the
corresponding points is at most

    D(s,t) = max_i ||p_i(s)-p_i(t)||.

Therefore the Hausdorff distance between T(s) and T(t)
is <=D(s,t). Distance from T to a FIXED compact set S
is 1-Lipschitz under Hausdorff distance, so

    |dist(T(s),S)-dist(T(t),S)| <= D(s,t).

For one full temporal interval [u,v] with midpoint m,

    dist(T(s),S) >= dist(T(m),S) -
                    (1/2)*max_i ||p_i(v)-p_i(u)||

for all s in [u,v].

A sufficient collision-free condition with thickness/skin h
is therefore

    dist(T(m),S) - max_displacement/2 > h.

The kernel computes a true finite-triangle distance at the
midpoint, applies a conservative numerical guard and checks
this strict inequality. A sufficiently separated swept-AABB
pair can be discarded even earlier. If the inequality
cannot establish separation, it recursively subdivides
time and checks each smaller interval.

At the depth budget limit the result is **UNRESOLVED**.
It is NOT a proof that contact occurred. Crucially, it is
also NOT permission to move the body through the obstacle.

Source:

    certify_moving_triangle_rigid_triangle(...)

Results include complete-separation, earliest conservative
unresolved prefix, interval-node count and sampled *possible*
contact counts. An unsigned Euclidean distance can NEVER be
negative, so zero-distance samples are not labeled "negative
penetration depth."

### Why an open triangle mesh has no inside/outside

This experiment's rigid patch consists of finite faces
with a boundary. The mesh is NOT a closed oriented volume.

As a result, an arbitrary point cannot be called physically
"inside" or "outside" this patch using a generic winding
sign without choosing additional semantics. The correct
demonstrated invariant here is:

    minimum nonnegative distance between any deformable face
    and any rigid finite triangle > 0,

together with no unresolved swept interval for each
accepted physical substep.

For a closed watertight rigid obstacle, a different
inside/outside and signed-contact interpretation would be
needed; for self-contact, topology masks and adjacent-face
exclusion rules would also be needed.

## 3. Real physical reaction on the original tetrahedral solid

Source:

    include/kyoto/rigid_mesh_fem_contact.hpp

At the beginning of each proposed time step dt:

1. Evaluate all original **3D hyperelastic tetrahedral
   neo-Hookean** internal forces and gravity.
2. Predict semi-implicit Euler velocities and apply true
   material damping:
   
       v_trial = exp(-gamma*dt) * (v_old+dt*M^-1*f).
3. For each potentially near deformable face versus rigid
   finite face, find the actual closest feature point.
4. Compute the contact normal from their world-space
   closest-point difference, NOT a sphere normal nor the
   constant global z axis.
5. Project the real deformable velocity into the actual
   barycentric contact coordinate and compute the effective
   inverse mass

       k = sum_i (w_i^2/m_i).

6. Apply a normal restitution impulse

       J_n = -(1+e)*u_normal/k,     if u_normal<0,

   followed by Coulomb-limited tangential impulse

       ||J_t|| <= min(mu*J_n, ||u_t||/k).

   Every nodal velocity receives the actual vector update

       delta_v_i = w_i*(J_n*n+J_t)/m_i.

   The rigid body is treated as static/infinite mass.
7. Construct the WHOLE deformable-body Euler motion trial.
   Check continuous separation for EVERY potentially
   interacting true deformable/rigid triangular pair.
8. If any interval is unresolved, REJECT the entire physical
   trial. Do not merely clamp its displacement and charge
   the full elapsed dt.
9. Check that every new volume tetrahedron remains
   strictly orientation-preserving with det(F)>1e-6.
   Reject any invalid trial instead of pretending it is
   a physically accepted deformed state.
10. For a fully safe trial, apply post-motion local face
    contact impulses, independently audit full rigid-mesh
    nearest distance, and commit only after all tests pass.

This is an explicit local Gauss–Seidel frictional response,
**not** a globally coupled contact optimization and not
IPC's implicit barrier energy solver. It has an activation
skin. It does not solve self-contact or moving rigid
mesh dynamics.

## 4. Physical-time replay, rejection, and rollback

Like Pass 8, a proposed whole step is either fully accepted
or *re-integrated at a genuinely smaller physical dt*:

    integrate(t,h):
       propose FEA motion + finite triangle contact over h
       test all swept finite triangle pairs

       if entire proposed h is certified and all tets valid:
           accept real h and write its exact receipt
       otherwise:
           integrate(t,h/2)
           integrate(t+h/2,h/2)

Every second half recomputes FEA forces and contact impulses
from the actual accepted first-half state.

A fixed maximum subdivision depth, leaf budget and minimum
physical dt prevent unlimited work. If the problem cannot
be resolved, the top-level function throws and leaves
**all of the original input Body and external receipt array**
unchanged. Rejected trial impulses NEVER leak into an
accepted physical state.

The sum of accepted genuine leaf dt values must equal
the original requested time, before the public physical
clock can advance.

Source:

    step_time_consistent_rigid_triangle_mesh(...)

This is a robust **finite-test time-accounting contract**,
but still needs genuinely machine-rounded CCD and
independent physically validated material-response models.

## 5. The actual experimental scene

The native reference rigid patch is:

    corrugated_contact_patch(resolution=7, height=.035)

Its 49 original 3D positions are constructed once and
connected into 72 finite triangles. The surface includes
a nonzero sloped, curved height variation. The contact
solver never treats it as an analytic heightfield:
only the original indexed triangles are passed to the
geometric narrow phase.

The original FEM source body is:

    toroidal_solid()

It contains 180 deforming vertices, 480 tetrahedra and
320 connected surface triangles. Physical source states
are generated by integrating gravity and true neo-Hookean
forces, not morphing an animation.

Numerical and physical acceptance gates in
tests/rigid_mesh_fem_contact_tests.cpp:

- A far-away rigid triangle patch must produce zero
  impulses and reproduce the old uncut free-flight FEM.
- A 50 m/s close-approach trial with subdivision disabled
  must fail without modifying physical state or receipts.
- With the actual finite curved patch in place, the
  solver must produce true physical face impulse events
  on a substantial FEM trajectory.
- Every accepted state must retain a positive nearest
  **FINITE TRIANGLE/FINITE TRIANGLE** distance.
- No tetrahedron may invert; elastic and kinetic
  energy must remain finite.
- Every macro timestep must integrate its full requested
  physical duration. Every actual accepted subdivision
  is counted.
- Original exact geometric audit and broadphase culling
  work are counted, rather than claiming a free speedup.

The first continuous rigid-mesh sweep tests also cover
an edge penetrating face INTERIOR, coplanar crossing
edges, coincident/tangent finite triangles, endpoint-safe
temporal tunnelling and 850 seeded separated moving
triangles with additional independent sampled sanity
checks. The latter samples are not a continuous proof.

## 6. Negative control and scientifically fair comparisons

Run the old Pass-8 **sphere-only physical solver** with
the exact same rigid triangle patch present for
independent geometric AUDITING. Since the old solver
has no rigid-mesh response, it may physically cross
those real finite faces. Preserve the count of its
intersecting source states; do not fake intersections
for an attractive chart.

Do NOT compare this different-obstacle sphere response
with the new mesh response as if the two implemented
an equivalent contact law. The valid conclusion is
that sphere-only response does not automatically
solve contact against a separate arbitrary finite
triangle obstacle.

Actual apples-to-apples physical accuracy requires
the SAME rigid mesh, material, contact thickness,
friction, forcing, time integration and physical
observable for all competing implementations.

Actual SCA performance requires the SAME primitive
trajectories, feature topology and numerical
tolerances against a PINNED official IPC Toolkit
LBVH baseline, with build, refit, active candidate
maintenance, narrow-phase evaluation, cache invalidation
and fallback all included in paid time.

Neither has yet been demonstrated.

## 7. Native reproduction and academic visuals

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

The 450-step contact test is in normal CTest.
For original native 900-step scientific evidence:

    ./build/kyoto_triangle_mesh_tests results/pass11-mesh
    ./build/kyoto_rigid_mesh_fem_tests results/pass11-mesh

Then plot only the native source files:

    python3 -m pip install numpy pandas matplotlib pillow plotly
    python3 scripts/render_pass11_real_triangle_fem.py \
       --input results/pass11-mesh \
       --output results/pass11-mesh/visuals

The experiment generates:
- rigid_triangle_physics_900.csv (901 source states,
  true energies, min det F, impulses, physical time);
- rigid_triangle_physical_vertices.csv (all original
  FEM 180 nodes at 91 genuinely simulated timesteps);
- deformable_surface_triangles.csv (320 original 3D
  FEM face connectivity rows);
- rigid_patch_original_vertices.csv and
  rigid_patch_original_triangles.csv (49 true 3D
  vertices, 72 original rigid finite faces);
- real_FEM_against_nonresponding_rigid_mesh.csv
  (old sphere-only physical motion audited against
  the SAME rigid patch);
- rigid_mesh_manifest.json (original exact
  topology, genuine physical response, solved
  scope, failure budgets and minimum geometry).

Scientific visuals include seven actual-source
figures and an atlas, an entirely OFFLINE Plotly
interactive full 3D source physical trajectory
with slider and play/pause controls, and a GIF
composed of selected original simulated frames,
not interpolated/synthesized physics.

The complete GitHub Actions workflow must pass
before the 900-step result is a verified
research measurement:

    kyoto-stage11-real-rigid-triangle-mesh-FEM-contact-evidence

All values and outputs must remain reproducible
from the original C++23 sources and explicit CI.

## 8. Actual verified initial physical results

**This section contains original measured C++23 experiment values, not
simulation results invented by a renderer.** The first complete
900-step scientific workflow on 23 November 2026 passed five jobs:
Ubuntu/macos Debug/Release and the original-data/visual pipeline.

https://github.com/swayam8624/kyoto-common-contact/actions/runs/45160203229

The original 901-state real nonplanar-mesh source archive reports:

| Native physical measurement | Verified result |
| --- | ---: |
| Actual physical 3D neo-Hookean integration steps | 900 |
| Total integrated elapsed time | 0.675 seconds |
| Deformable body original volumetric vertices | 180 |
| Real tetrahedral neo-Hookean elements | 480 |
| Original deformable exterior triangle faces | 320 |
| Actual static rigid triangle-mesh vertices | 49 |
| Original nonplanar rigid finite triangular faces | 72 |
| First real face-contact impulse | 0.21975 seconds |
| Routine steps with real rigid-mesh impulses | **517 / 900** |
| Actual normal/tangential barycentric face impulse updates | **11,731** |
| Accepted actual physical substeps | 900 |
| Rejected routine geometric trials | 0 |
| Rejected routine inverted tetra trial | 0 |
| Minimum full finite deformable–rigid triangle separation | **1.736230314 mm** |
| Minimum tetrahedron Jacobian det(F) | **0.722870523** |
| Maximum genuine Cauchy von Mises stress | **8,987.93 Pa** |

All 901 accepted original source states passed a FULL
finite-face/finite-face static surface distance audit;
all 900 requested physical timesteps were actually
integrated. This specific ordinary physical trajectory
needed no adaptive subdivision because the precontact
barycentric impulses maintained safe clearance; do NOT
attribute all safety to the continuous rejection alone.

The EXACT same 72-face nonplanar rigid patch was then
audited along an earlier sphere-only physical trajectory.
Because the earlier sphere solver does not react to the
separate rigid mesh, the independent finite-triangle
auditor found **95 / 181 sampled physical states** with
true intersecting deformable/rigid face pairs, across
**11,465 original intersecting face pairs**. The
minimum measured finite-face distance was zero.

This is a valuable *negative control*, not a matched
physics or speed benchmark. The existing sphere collision
response was designed for a DIFFERENT obstacle and
therefore fails when physically unmodeled finite faces
are introduced.

The real physical trajectory, original 3D mesh vertices,
rigid mesh connectivity, source energies, tetrahedral
Jacobian, FEM stress, original collision impulse, seven
source-backed science plots, full offline 3D viewer
and 24-frame original-physics GIF were all archived.

The source archive's exact measured results—not any
claims in this document—remain authoritative if
reproduction on other hardware yields differences.

## 9. Remaining gates



**A. Geometric numerical robustness:** Our moving
triangle/triangle method proves separation in exact
arithmetic. A scale-aware binary64 guard is used,
but the new finite mesh solver has NOT yet inherited
Pass 10's outward IEEE interval certification for
arbitrary moving triangles versus static triangles.
Robust degenerate and extreme floating-point
feature predicates and independent reference
VF/EE CCD are still needed.

**B. Closed shells, moving rigid meshes and
self-contact:** The patch is open and stationary.
There is no simultaneous moving-deformable/moving-rigid
CCD, closed-shell interior classification, frictional
implicit contact, self-contact or robust manifold
clustering at this stage.

**C. Physical accuracy:** Positive tetra Jacobians
and nonpenetrating surfaces do not establish a
correctly calibrated energy budget, stable
convergence order, friction law or experimental
material behavior. Strong independent physical
benchmarking remains necessary.

**D. Central broadphase research:** A reliable
maintained, adaptive Spacetime Contact Atlas with
a fair FULL-PAID comparison against pinned official
IPC Toolkit LBVH on held-out contact-rich 3D FEM
trajectories has NOT been established. Continue
separating physics correctness from performance
claims and from novelty claims.
