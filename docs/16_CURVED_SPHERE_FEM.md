# Pass 6 — True 3D FEM on a curved rigid surface, with full-surface falsification

**Experimental status:** Implemented in C++23, guarded by native physics
tests and source-export CI. The raw GitHub Actions receipts are authoritative.
A passing build does NOT imply the solid surface is guaranteed contact-safe.

## 1. What genuinely changes from the flat-floor case?

Pass 5 integrated a connected 3D tetrahedral neo-Hookean torus under
gravity. A horizontal analytical constraint z>=0 was sufficient
because a triangle is a convex combination of its vertices:
if all three vertex z coordinates are nonnegative, the entire
triangular face is nonnegative.

That argument does **not** generalize to a curved exterior constraint.

A sphere of center c and radius r occupies

    S = {x in R^3 : ||x-c|| < r}.

Its exterior {x : ||x-c|| >= r} is NOT convex. Three vertices can
all lie outside S while their triangle passes straight through it.

This failure is central, not a visualization detail.

Pass 6 adds a smooth, static, curved rigid sphere at
c=(0,0,-0.45) metres, r=0.75 m, as a separate collider.
The same physically integrated 3D toroidal solid falls under
gravity with real material forces; NO positions are scripted.

Material and integrator remain the documented Pass-5 reference:
rho=360 kg/m^3, E=22,000 Pa, nu=0.32, g=9.81 m/s^2,
explicit-force semi-implicit Euler, dt=.00075 seconds,
900 actual solver steps. Contact uses sphere restitution .1
and friction coefficient .24, not experimentally validated materials.

## 2. Analytical swept VERTEX vs rigid sphere

For a single accepted linear trial chord, define

    x(t) = p + t d,   t in [0,1],  d = q-p.

At the smooth sphere boundary, solve

    ||p + t d - c||^2 = r^2.

Let y=p-c. This is the quadratic

    A t^2 + B t + C = 0,

    A=d·d,    B=2 y·d,    C=y·y-r^2.

For an initially outside point and inward approach B<0,
a real first touching time may exist if

    D=B^2-4 A C >=0.

The smaller real root is

    t_hit = (-B - sqrt(D))/(2 A).

The numerically more stable equivalent for B<0 is

    t_hit = 2 C/(-B+sqrt(D)).

To avoid a future false negative from only checking endpoint
penetration, this analytical root catches a vertex traveling
fully across the sphere during one timestep, with both start
and end positions outside the sphere.

A valid first root in [0,1] is retained. The native solver
accepts only a small pre-impact prefix

    t_safe = max(0,t_hit - 1e-9)

of the proposed linear vertex chord and applies the calculated
normal impulse to the velocity. A tangential friction impulse is
bounded by the coefficient times the normal impulse.

**Important:** This is mathematically a vertex/chord intersection,
not robust directed-rounded interval arithmetic for all possible
binary64 inputs. The numerical regression suite includes 3,000
seeded random chords, outside-to-outside tunneling, near misses,
tangency, initial penetration and boundary movement. This is finite
test evidence, not a proof of a machine-correct root for every input.

## 3. Physical integration and contact approximation

The same first-Piola neo-Hookean element forces and mass assembly
as Pass 5 are used. Trial velocity is obtained from the FEM
elastic force, gravity and damping.

An impacted vertex is stopped conservatively before crossing the
rigid sphere, and its velocity obtains a normal restitution impulse
and a bounded tangential friction impulse.

This is a **first-order, lumped vertex-contact integrator**:
- it truncates the remaining part of the *impacted vertex's*
  trial displacement instead of globally re-solving that full
  integration step;
- it does not solve coupled contact constraints at face
  quadrature points, edges or a global barrier potential;
- it can introduce energy errors in addition to explicit time
  integration, damping and friction;
- it does not enforce no-interpenetration of the complete surface;
- it has no contact-resolved self-intersection constraint.

The claim is therefore precise: **a physically integrated 3D
hyperelastic body with numerical vertex/sphere contact**.
It is NOT a fully correct general rigid/deformable contact solver.

## 4. The independent full-triangle geometric audit

For sphere center c and a mesh triangle T=(a,b,d), define

    delta_T = min_{x in T} ||x-c|| - r.

When delta_T is negative, the triangle enters the sphere's interior.
The minimum is evaluated over:
- all three vertices;
- all three closed edge segments, including their clamped
  closest-point projections;
- the orthogonal projection of c onto the triangle's plane
  **if** its barycentric coordinates fall inside the triangle.

The implementation is in
include/kyoto/sphere_surface_audit.hpp.
It handles topologically valid triangles that are geometrically
degenerate by falling back to their edges.

A deliberately adversarial geometry has vertices at distance 2
from the origin, yet spans the origin in its interior. Against a
unit sphere, all vertices have positive clearance while the
triangle clearance is -1. This independently falsifies the
incorrect assertion "vertex-safe implies triangle-safe."

The exporter computes this exact finite-triangle closest-point
quantity on **all 901 accepted physical states** (t=0 through 900
steps). It records the minimum signed vertex clearance and minimum
signed surface clearance separately, as well as the count of
overlapping connected faces in every accepted state.

This is a rigorous geometrical minimization for a given piecewise
linear surface in real arithmetic, with ordinary floating-point
limitations. It is NOT a complete continuous triangle-sphere CCD
between states and does NOT imply an acceptable physical correction.

The scientifically correct interpretation of the audit output:

    states_with_vertex_inside_sphere == 0

may hold simultaneously with

    states_with_static_triangle_sphere_overlap > 0.

Such a result would be a **known failure of the present
vertex-only contact response**, not evidence that the analytical
sphere equation or the independent audit is wrong. Report the
number directly and keep the negative cases.

## 5. Reproducible native sources and visual outputs

C++23:
- include/kyoto/curved_contact.hpp — analytic per-vertex swept root
  and first-contact impulse law;
- include/kyoto/sphere_surface_audit.hpp — full finite-triangle
  nearest-point audit;
- tests/curved_contact_tests.cpp — analytic ray-root tests, 3,000
  random chords, 900 real physical steps;
- tests/sphere_surface_audit_tests.cpp — Voronoi/triangle test,
  specific interior failure geometry, 900 real state checks;
- apps/curved_fem_export.cpp — trace source and manifest.

Native artifacts:
- curved_solid_states.csv — 181 actual 3D FEM states with velocity,
  von Mises Cauchy stress, energy density and vertex clearance;
- curved_solid_metrics.csv — energy, displacement, deformation
  volume ratio, normal impulses, center-of-mass motion;
- curved_contact_impulses.csv — actual contact-bearing vertex-step
  records with impulses N*s;
- surface_sphere_clearance.csv — **901 independent full-surface
  safety observations**, including negative outcomes;
- surface_faces.csv, tetrahedra.csv — original 3D connected mesh;
- curved_manifest.json — physical parameters, scope and negative
  findings.

The Python renderer produces a scientifically traceable atlas:
1. 3D volumetric torus and curved obstacle at four real solver times;
2. synchronized vertex-versus-full-triangle signed clearance curves;
3. actual solver impulse history alongside unresolved face overlap;
4. kinetic, elastic and gravitational energy;
5. Cauchy von Mises stress and det(F) deformation;
6. 3D native impulse spatial distribution;
7. offline rotatable true FEM surface replay with a static sphere;
8. 181 ParaView **tetrahedral volume** states and scientific scalars.

The volume/velocity values are real C++ FEM outputs, not artistically
fabricated trajectories. The camera, shading and sphere transparency
are presentation only.

Reproduce:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/kyoto_curved_fem_export results/curved-fem

    python3 -m pip install numpy pandas matplotlib plotly pillow
    python3 scripts/render_curved_fem.py \
       --input results/curved-fem \
       --output results/curved-fem/visuals

All these are archived by the source-verification workflow.

## 6. What is necessary before publishing contact-correct solver results?

First replace vertex-only sphere contact with a **conservative
whole-face and edge narrow phase**, applied to the motion of all
connected mesh features, not only a single vertex. A candidate
approach is globally synchronized conservative advancement for
the sphere/triangle signed distance. A proof must establish that
the minimum distance of a moving triangle to the static sphere
cannot decrease faster than the maximum vertex speed:

    |distance(T(t),c) - distance(T(s),c)|
       <= max_i ||p_i(t)-p_i(s)||,

when vertices are linearly interpolated between identical
connectivities. This follows by using the same barycentric
combination of moving vertices.

Then the solver must:
- choose a provably safe substep/contact fraction for the
  entire surface, including grazing and already-touching cases;
- compute contact normals at the nearest point, and distribute
  constraints/impulses through barycentric participation;
- conserve or deliberately dissipate momentum with a stated
  contact law, maintain strictly positive tetrahedra, and
  audit energy and surface penetration every step;
- test self-contact and moving nonplanar rigid meshes;
- compare an independent known official IPC CCD implementation,
  not a duplicated in-house narrow phase;
- independently assess numerical rounding near tangency.

Only after robust general contact behavior should SCA's
incremental broad-phase frontier be benchmarked against the
already-integrated pinned upstream IPC Toolkit LBVH on genuine
contact-rich deformable scenes. No claimed speedup or novelty
has been established in this Pass 6.
