# Pass 5 — First physically integrated 3D volumetric deformable body

**Research objective:** replace scripted kinematic test surfaces with a true
3D continuum-mechanics trajectory and trustworthy actual physical fields.
This is a **reference implementation**, not production implicit FEM or a new collision algorithm. Its full native physics + science visualization workflow passed on macOS and Ubuntu in Debug and Release, plus the Ubuntu source-artifact job: https://github.com/swayam8624/kyoto-common-contact/actions/runs/45072204086

## 1. Physical specimen and topology

Construct a closed, genuinely volumetric toroidal solid with rectangular
cross-section. The geometry is *not* a paper-thin cloth, a spherical
visual prop, or a per-frame animated mesh.

Rest-coordinate parameterization for angular coordinate theta and
cross-sectional coordinates u,v:

    X(theta,u,v) = ((R+u) cos theta, (R+u) sin theta, H+v).

R=0.70 m, u,v in [-0.18, 0.18] m, H=0.52 m.

The lattice is periodic in theta with 20 ring samples and has
3x3 nodes across the section, giving 180 distinct vertices and a
strictly closed seam. Each of 20 x 2 x 2 hexahedral cells is
tetrahedralized into six tetrahedra, yielding 480 volumetric elements.
The connected boundary contains 320 triangles. The tetra reference
orientation is explicitly checked and corrected before volume and
mass assembly.

The specimen has NO anchored nodes. It begins with zero velocity above
the floor, then physically accelerates, deforms, and impacts.

Initial material parameters:
- Mass density rho = 360 kg/m^3.
- Young's modulus E = 22,000 Pa.
- Poisson ratio nu = 0.32.
- Gravity 9.81 m/s^2 downward.
- Damping coefficient 0.45 s^-1 (phenomenological).
- Rigid-plane restitution e=0.12.
- Lumped contact friction coefficient 0.28.

This is an example soft-solid parameter set, NOT validated
experimental data for a named real elastomer.

## 2. Hyperelastic constitutive law — no invented animation

For a tetrahedron, let Dm be its three independent rest-edge vectors
as columns, and Ds the same current-edge differences. The deformation
gradient is

    F = Ds inv(Dm),       J = det(F) > 0.

The compressible neo-Hookean energy density (J/m^3) is

    W(F) = mu/2 * (F:F - 3) - mu*ln(J) + lambda/2 * ln(J)^2.

The Lamé parameters are calculated from E and nu:

    mu = E/[2(1+nu)]
    lambda = E nu/[(1+nu)(1-2nu)].

The first Piola-Kirchhoff stress is derived analytically:

    P = dW/dF
      = mu*(F - F^{-T}) + lambda ln(J) F^{-T}.

Let grad_X N_i denote the reference-space shape-function gradient
for vertex i. The tetrahedral elastic force is

    f_i = -V0 * P * grad_X N_i,

where V0 is positive rest volume, not current deformed volume.
The four shape-function gradients sum to zero, so every element's
internal forces sum exactly to zero in exact arithmetic. The
implementation derives forces by matrix multiplication and assigns
f0=-(f1+f2+f3) for better numerical balance.

The *Cauchy* stress (Pa) is

    sigma = (1/J) P F^T,

and the von Mises invariant is calculated from its components.
The nodal displayed stress is the rest-volume-weighted mean
of incident tetra von Mises stresses. This is a genuine calculated
material stress scalar, not a decorative displacement proxy.

Material energy in joules is the integrated rest-volume sum

    U_elastic = sum_tetra V0 W(F).

There is no velocity-dependent force disguised as elasticity.

## 3. Mass and integrator

Each tetra mass rho*V0 is distributed equally to its four vertices.
Consequently the total assembled mass equals rho times the total
reference tet volume; the solver checks this exact accounting.

With a lumped diagonal mass matrix and elastic+gravity force,
the reference solver uses semi-implicit Euler at fixed dt:

    v_{n+1}^trial = exp(-c dt) *
        [ v_n + dt M^{-1} f_elastic(x_n) + dt g ];

    x_{n+1}^trial = x_n + dt v_{n+1}^trial.

Stage-5 test and visual export currently use
dt=0.00075 s and 900 integration steps (T=0.675 s).

This is explicit-force time integration, NOT a fully implicit
Newton-contact energy minimization. It can be unstable if dt is
too large. Inverted/invalid tetrahedra trigger a failure rather
than being silently clamped.

## 4. Contact is real, but restricted to one analytically simple shape

A rigid infinite halfspace requires each vertex to obey z>=0.
The trial displacement during a time step is linear. Therefore,
if z_n>=0 and z_trial<0, the exact trial line crossing of the
plane occurs at

    alpha = (z_n - z_floor) / (z_n - z_trial).

The solver projects the final vertex z coordinate to the floor.
It applies a lumped normal impulse for a closing normal velocity:

    J_n = -(1+e)*m*v_n^trial       if v_n^trial<0,

then sets v_after,z = -e*v_n^trial.
It reduces tangential velocity by a Coulomb-like frictional impulse
bounded by mu_contact * J_n.

The native floor events in the exported CSV are real results of
this numerical contact model and have units N*s.

This contact law is **NOT** general mesh-mesh continuous CCD, an
official IPC barrier potential, a fully coupled constraint
optimization, or verified self-contact response. No claim is made
that arbitrary triangle surfaces cannot intersect one another
in the current reference simulation. The conservative plane
restriction is analytically correct for this flat floor only.

## 5. What is actually tested

The native C++ tests cover:
- rest-shape elastic energy and no spurious rest forces;
- rotation and translation invariance of W(F);
- assembled internal force balance;
- finite-difference derivative of elastic energy against
  independently calculated analytic force;
- reference mass = density * rest volume;
- 100-step gravity-only ballistic COM, compared with the exact
  semi-implicit Euler discrete gravity formula;
- 900 true simulation steps with unilateral floor contacts;
- no vertex allowed below the plane at accepted output states;
- strictly positive tetrahedra J;
- finite kinetic/potential/elastic energies;
- nonzero deformation and material stress.

These are necessary regression checks, not independent certification
that every detail of real-world elastodynamics is reproduced.
Free-fall COM is a stronger quantitative check than visual motion.

## 6. Source trace and visual fields

The C++ executable kyoto_solid_simulation exports:
- solid_states.csv: 181 solver-generated frames with x,v, rest x,
  mass, displacement, nodal volume-weighted energy density and
  actual von Mises stress in Pa;
- solid_tetrahedra.csv: actual connected tet vertices and V0;
- solid_surface_faces.csv: actual topological exterior triangles;
- floor_impacts.csv: actual nonzero contact impulses with step and xyz;
- solid_metrics.csv: timed kinetic, elastic, gravity energies,
  minimum J, peak von Mises, center of mass, height, impulse count;
- solid_manifest.json: material, dt, 900 steps, all scientific limits.

The scientific renderer derives:
1. Real kinetic/elastic/gravity/total energy curves;
2. Cauchy von Mises stress vs minimum deformation Jacobian;
3. Actual integrated normal contact impulse time series;
4. Contact footprint as spatially weighted impulse histogram;
5. True center of mass vs analytically ballistic trajectory;
6. Nodal von Mises distribution at five physically integrated times;
7. Interactive 3D actual tet-surface stress animation;
8. 181 **volumetric tetrahedral** ParaView data snapshots with
   velocity/displacement vectors, stress and energy fields.

A separate VTK cinematic studio renders true simulated states with
calibrated scalar range, an interior tetrahedral cutaway and a short
animated loop. Color bars and stress captions are physical, but
lighting, ground-grid extent, and camera are presentation choices.

None of the figures should be labeled an external IPC solver, a
general-contact physical simulation, or a new algorithm speedup.

## 7. Reproduction

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/kyoto_solid_simulation results/solid-fem
    python3 -m pip install numpy matplotlib pandas plotly pillow
    python3 scripts/render_solid_fem.py \
       --input results/solid-fem \
       --output results/solid-fem/visuals

For the VTK studio (offscreen VTK/GL installation required):

    python3 -m pip install vtk matplotlib pillow numpy
    python3 scripts/render_solid_vtk_studio.py \
       --input results/solid-fem \
       --output results/solid-fem/vtk-studio \
       --gif

Open the PVD time series in ParaView to examine tetrahedral
volume Jacobians and material stress. Keep the exact source
commit and raw logs together with benchmark data.

## 8. Next scientific acceptance gates

1. **Independent IPC narrow-phase auditor of the genuine solver
   trajectories:** compare all mesh pairs and plane interactions,
   count unintended self-intersections, explain any.
2. Use official IPC on a nonplanar connected obstacle, with a
   verified collision-free line search / response. Plane-only
   projection cannot establish general-contact correctness.
3. Compare energy/force quality with an independent FEM package or
   analytic manufactured solutions; test mesh refinement
   convergence and timestep convergence.
4. Introduce stable self-contact, friction and mixed
   rigid/deformable interaction without hand-animated motion.
5. Only then implement the incremental SCA hierarchy and test its
   complete paid collision+solver step performance against official
   LBVH and independent data.

The value of Stage 5 is the existence of actual generated
physical trajectories and fields, not yet a claimed novel solver.


## 9. Verified first native physics receipt (actual GitHub runner)

Raw artifact: kyoto-stage5-real-elastic-FEM-physics, ID 14772749894,
from successful cross-platform run 45072204086. Generated artifacts
include CSV evidence, 6 analytical science figures, an offline
interactive physically integrated 3D volume surface, and 181
ParaView **volumetric tetrahedral** time-series states.

The actual original C++ native manifest and CSVs report:

| Metric | Result |
|---|---:|
| Native integration steps | 900 |
| Physical duration | 0.675 s |
| Full 3D FEM vertices / tetrahedra | 180 / 480 |
| Boundary triangles | 320 |
| Reference volume | 0.5606804346 m^3 |
| Conserved lumped mass | 201.8449565 kg |
| Distinct exported physical states | 181 |
| First floor impulse | approximately 0.2685 s |
| Vertex-step ground impulse events | 10,380 |
| Export intervals containing impulses | 46 |
| Maximum observed von Mises stress | 7,017.9286 Pa |
| Minimum observed det(F) | 0.7402905 |
| Initial mechanical energy | 1,029.6515 J |
| Mechanical energy at t=0.675 s | 498.5716 J |

Each "vertex-step ground impulse event" is a single vertex receiving a
normal impulse on one accepted integration step. It is **not** a
distinct macroscopic collision episode. The repeated count includes
persistent grounded nodes under a unilateral contact law.

The energy decline includes the configured viscous damping, restitution
loss and tangential friction; it does not establish high-order energy
conservation or physical calibration. The stress scalar derives from
the **Cauchy tensor** of the implemented neo-Hookean model and is
reported in Pa, not an arbitrarily colored kinematic proxy.

The native exporter, figure generator and test suite succeeded in the
verified CI run; the separate VTK cinematic renderer has its own
independent Actions verification workflow.
