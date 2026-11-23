# Pass 7A — Globally synchronized full-triangle contact in a real 3D FEM body

**Research scope:** This is a reproducible *finite-test geometric safety*
improvement over the earlier vertex-only solver. It is not a new general
IPC solver, an exact machine-interval implementation, a fully implicit
contact-constrained integrator, a self-contact solution, or an SCA
hierarchy speedup.

## 1. Problem statement: a concrete native falsification

The Pass-6 true tetrahedral neo-Hookean torus (180 vertices, 480
tetrahedra, 320 boundary triangles) is integrated by material forces
and gravity, not by scripted vertex paths.

The smooth sphere has center (0,0,-0.45)m and radius 0.75m. The old
vertex-only analytic swept CCD and response prevents a vertex entering
the sphere, but it does not prevent the interior of a connected face
from intersecting it.

That old solver has a frozen, source-backed negative reference:

    901 accepted states audited;
    0 states with an interior vertex;
    458 states with one or more penetrating triangle interiors;
    -0.0210998841 m worst finite-triangle sphere clearance.

The discrepancy is not noise or unfair choice of scalar: the sphere's
EXTERIOR is nonconvex, so the triangle interior can cross it while
each endpoint remains outside. Any contact approach that uses only
point-versus-obstacle checks must pass this counterexample.

Pass 7A does **not** replace or rewrite that original dataset.

## 2. The continuum material and time integrator are unchanged

The volume still uses compressible neo-Hookean elasticity:

    F = Ds inv(Dm),  J = det(F)>0,
    W(F) = mu/2*(F:F-3) - mu log(J) + lambda/2*(log J)^2.

    P = mu*(F - F^{-T}) + lambda*log(J)*F^{-T}.

The 3D tetrahedral forces are computed by the analytical energy
gradient, with diagonal lumped masses and gravity. Before contact,
a semi-implicit Euler trial is:

    v*_i = exp(-damping*dt)
           [v_i + dt*(f_elastic_i/m_i + gravity)].

    x*_i = x_i + dt*v*_i.

The initial material parameters and 900 fixed-length dt=.00075
steps are kept the same as the negative-control run.

## 3. Contact at the entire triangle, NOT its vertices only

For every connected surface face T=(i,j,k), find the closest
position q on the FINITE triangle to the rigid sphere center c,
not merely the plane containing the face.

The independent closest-point code evaluates the triangle's
interior, its three edges, and its vertices, returning the
barycentric coordinates w_i,w_j,w_k of q.

At near-contact, its outward unit normal is

    n = (q-c)/||q-c||.

The face-relative contact velocity is the barycentric mean

    u = w_i v_i + w_j v_j + w_k v_k.

With diagonal vertex masses m_i, effective inverse contact mass is

    k = sum_face(w_i^2/m_i).

Thus, to change the normal relative velocity by Δu_n, the
mass-consistent impulse is

    J_n = Δu_n/k.

For an incoming normal velocity u_n=u·n<0 and restitution e,

    J_n = -(1+e)*u_n/k.

Each participating vertex obtains

    Δv_i = (w_i/m_i)*J_n*n.

This distribution is derived from the diagonal mass matrix.
It is not three arbitrary positional displacements. For a fixed
obstacle, the opposite momentum impulse is delivered to the
infinitely massive rigid body, though this reference model does
not integrate the rigid body's state.

For tangential velocity u_t=u-n(u·n), Coulomb-like impulse is
limited by

    J_t = min(mu_friction*J_n, ||u_t||/k),

applied opposite the direction of u_t. The code records the
normal and tangential magnitudes separately. An analytical
three-vertex unequal-mass unit test verifies the effective
mass, normal velocity cancellation and each barycentric
impulse share.

**Limitations:** contacts are solved sequentially in stable face
order, with a configurable number of Gauss–Seidel iterations.
This is not a globally implicit complementarity/KKT solve and is
not proven order-independent or physically calibrated.

For the first reference, restitution is 0, friction coefficient
0.2, and an activation shell starts 2mm outside a 0.5mm
conservative skin. This is a *compliant standoff contact
approximation*, not a perfect zero-gap hard-contact law.

## 4. The globally synchronized continuous time gate

A local face response cannot make the entire connected volume safe
unless the positions also move consistently in time.

For every actual proposed time-step chord from x_i to x*_i,
invoke the full-triangle Lipschitz certificate from Pass 6B.
Let α_f denote that face's last certified-clear prefix in [0,1].

Define ONE global accepted position fraction:

    α = min_{all boundary triangles f} α_f.

Accept the proposed displacement through

    x_i,new = x_i + α*(x*_i-x_i)

for EVERY vertex of the 3D volume.

No face gets its own private time coordinate. The global position
is therefore synchronized, unlike local independent clamping
of 180 vertices.

The certificate on each face proves, **in exact arithmetic**,
clearance against an effective radius r+skin on [0,α].
The skin is 0.0005m (0.5mm) in these reference experiments.
This leaves margin for ordinary floating-point evaluation
without claiming formally directed-rounded certification.

The solver also applies one more sweep of barycentric contact
velocity impulses at the *accepted* geometry to arrest new
inward relative velocities near the sphere.

### Important physical approximation

If α<1, the implementation truncates the proposed position
displacement but advances its wall-clock integration time by the
full dt. It does **not** correctly reintegrate the remaining
(1-α)dt with updated forces. This is conservative geometrically,
but it changes momentum/energy in ways a time-consistent
coupled physical solver should not.

That trade-off is reported explicitly:

- accepted_global_fraction on every step;
- time_clipped boolean;
- normal/tangential impulses from the barycentric solver;
- number of tested time intervals and unresolved faces;
- energetic and deformation histories.

**Do not call this a finished physical time-of-impact integrator.**
The next solver pass must reconcile the remaining fractional time,
multiple close contacts and material forces without nonphysical
global stoppage.

## 5. All-state independent postcondition

The proposed step is kept transactional: new node positions and
velocities are committed only after all checks pass.

Before commit:

1. All oriented tetrahedra must have J>1e-6.
2. The original independent *full-triangle nearest-point* oracle
   audits all 320 boundary triangles, not merely vertices.
3. Every triangle must have positive clearance, with margin
   at least half the configured skin.
4. Node positions and velocities must be finite.
5. Any failure raises immediately; no evidence is silently repaired.

Every test step repeats this independent audit.

### Two different experiments are necessary

**A. Routine impact:** integrate the original falling toroidal
solid over 900 steps. Expect actual material deformation,
mass preservation, contact impulses and zero accepted face
overlaps in all 901 observed states.

Anticipatory barycentric impulses may be sufficient to make every
routine trial already safe. In that event, the global certificate
is still *checked* but need not clip any timestep. This must
not be misreported as proof that clipping caused the observed
safety improvement.

**B. Extreme tunnelling falsification:** start from the same
fully separated elastic solid, but give all vertices a fast
downward velocity of 900m/s. Its one-step trial chord traverses
a large portion of the sphere. The globally synchronized
certificate MUST return α<1 and nonzero unresolved-face
intervals. The accepted entire volumetric surface must
remain separated and every tetrahedron valid.

The second test distinguishes whole-face continuous protection
from a contact shell that only prevents gentle impacts.

## 6. Source files and reproducible receipts

Core:
- include/kyoto/whole_face_contact_dynamics.hpp
- tests/whole_face_contact_tests.cpp
- apps/whole_face_fem_export.cpp

Scientific display:
- scripts/render_whole_face_fem.py
- scripts/compare_curved_contact_solvers.py

The C++ exporter emits all original physical fields from the new
solver: 181 actual stress/velocity/energy-state frames, connected
surface faces and tetrahedra, true barycentric impulse records,
and all 901 exact-time native full-face audit observations.

Additional `whole_face_contact_step_diagnostics.csv` records
every one of the 900 accepted global fractions, near-contact
constraints, normal/tangential impulses, and paid interval
subdivision counts.

An A/B script reads both independent actual C++ FEM outputs
without altering either; it generates:
- complete 901-state old/new signed clearance comparison;
- original vs guarded overlap counts on every accepted state;
- original vs guarded energy and body-COM histories;
- 181-frame matched-time vertex trajectory differences.

These are correctness and physical-difference comparisons, not
wall-clock speed claims. Different contact laws produce different
trajectories.

## 7. Run all gates

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

    ./build/kyoto_curved_fem_export results/curved-fem
    ./build/kyoto_whole_face_fem_export results/whole-face-fem

    python3 -m pip install numpy pandas matplotlib plotly pillow
    python3 scripts/render_whole_face_fem.py \
      --input results/whole-face-fem \
      --output results/whole-face-fem/visuals

    python3 scripts/compare_curved_contact_solvers.py \
      --old results/curved-fem \
      --new results/whole-face-fem \
      --output results/whole-face-comparison

The cross-platform C++ proof is validated on Ubuntu and macOS,
Debug and Release. The slower visual-proof Actions job must
successfully reproduce every original CSV, all seven physics
charts, the interactive 3D replay, 181 volumetric ParaView
snapshots and both independent 901-state comparison tables.

The repository's original result artifacts, workflow run
statuses and complete manifests are the evidence source.
Do not infer job success from a source commit.

## 8. Scientific next gates

- Independent **full continuous** face/sphere audit on every accepted
  linear trajectory segment, with IEEE directed rounding.
- A time-consistent global continuous contact step that re-integrates
  the unused fraction of physical time with updated material
  forces and coupled contact constraints; no fake full dt.
- A globally coupled nonpenetrating contact solver with correct
  multiplier/impulse distribution and quantifiable energy loss.
- Separate self-contact and triangle-triangle arbitrary rigid
  mesh CCD with official IPC Toolkit and a second physical solver.
- A *fair fully paid* SCA incremental-frontier versus the official
  IPC v1.6.1 LBVH using independent realistic deformable scenes.
- All negative physical-dynamics/geometry outcomes retained.

This pass proves a significant tested geometric correction.
It does not yet establish general solver contact correctness,
whole-solver speedup or publishable incremental SCA novelty.
