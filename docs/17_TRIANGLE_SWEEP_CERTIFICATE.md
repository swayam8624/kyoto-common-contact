# Pass 6B — A conservative continuous triangle–sphere clearance certificate

**Status:** new C++23 narrow-phase certificate implemented, with native tests.
This is a *geometric time interval proof mechanism*, not a physical contact
response. In exact arithmetic, the mathematical guarantee below is
one-sided: a certified-clear interval contains no triangle–sphere contact.
Ordinary binary64 arithmetic remains a documented limitation.

## 1. Why the vertex-only model failed

The preceding curved FEM solver integrated a real 3D toroidal solid for
900 steps. Its per-vertex swept sphere intersection test reported zero
accepted vertex penetrations. But the independent surface audit recorded:

- 901 accepted simulation states, including the initial state.
- 458 states with at least one penetrating triangular face.
- 28,100 total penetrating-face/state observations.
- Minimum triangle-to-sphere signed clearance of -0.0210998841 m.
- Zero states containing a vertex strictly inside the sphere.
- First observed static face intrusion at step 302, t=0.2265 s.
- First vertex impulse at t=0.2295 s.

These are direct **negative observations** from the native exporter,
verified by source-backed GitHub Actions on Ubuntu and macOS in
Debug/Release:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/45133327667

The bad inference is:

    all vertices outside convex solid ⇒ all triangle interiors outside.

The implication is FALSE, since the *exterior* of a sphere is nonconvex.
The solver currently lacks whole-face contact response. This defect must
be repaired before claiming nonplanar solver contact correctness.

## 2. A useful rigorous one-sided bound

For a moving triangle T(t) with synchronized **linear** vertex
paths p0(t),p1(t),p2(t), define the scalar distance

    d(t) = min_{q∈T(t)} ||q-c||,

where c is the center of the static sphere of radius r.
An actual intersection occurs whenever d(t) <= r.

For times s,t in the same motion interval, each triangle point at
time s is a convex combination of its three vertex positions:

    q(s) = ∑_i w_i p_i(s),  w_i≥0, ∑_i w_i=1.

The *same* barycentric weights define a corresponding point
q(t) on the triangle at time t. Therefore,

    ||q(t)-q(s)|| ≤ ∑ w_i ||p_i(t)-p_i(s)||
                    ≤ max_i ||p_i(t)-p_i(s)||.

Take the nearest point on T(s), then interchange s and t. This
shows the distance-to-center functional is Lipschitz:

    |d(t)-d(s)| ≤ max_i ||p_i(t)-p_i(s)||.

For endpoints of a subinterval [a,b], denote

    L_[a,b] = max_i ||p_i(b)-p_i(a)||.

Because paths are linear, for its midpoint m=(a+b)/2:

    d(t) >= d(m)-0.5 L_[a,b],  for every t in [a,b].

Thus the complete time interval is provably collision-free when

    d(m) - 0.5 L_[a,b] > r.

This is a **full moving triangle** statement; unlike endpoint-only
vertex-sphere tests, it accounts for interior barycentric contact
at every synchronized time within the subinterval.

## 3. Computational implementation

Implementation:
include/kyoto/triangle_sphere_sweep.hpp.

At each candidate time interval:

1. Compute the three moving vertices at the interval midpoint.
2. Compute the true finite-triangle closest point to the sphere
   center, including the interior, edges and vertices.
3. Compute the maximum three vertex displacements across the
   complete interval.
4. If the strict midpoint-Lipschitz separation bound exceeds
   sphere radius plus a numerical margin, certify this entire
   interval separated.
5. Otherwise split the interval in half and recurse on the
   earlier half first.
6. At the resolution limit, return the **start of the first
   unresolved interval** as a conservative safe-prefix fraction.

All previously traversed intervals have been certified clear.
The routine never silently marks an unresolved interval as safe.
The caller can choose a higher maximum depth if the returned
prefix is too loose; depth is bounded.

Its result reports:
- entirely_separated — every interval is geometrically cleared;
- certified_safe_prefix — earliest time through which the
  traversed interval chain is certified separated;
- first_unresolved_upper — end of earliest unresolved leaf;
- sampled_overlap — a sampled midpoint actually has penetration,
  which is a positive witness, not the proof of absence;
- interval_tests, pruned_intervals, subdivisions — paid work.

Floating point: distance and dot products are currently ordinary
binary64 operations with a margin of roughly 1e-12 times scale.
**This is not yet a formally rounded machine certificate.**
The strict exact-arithmetic theorem is sound; converting it to
a universal machine-precision guarantee requires a directed-rounded
interval implementation and independent numerical audit.

## 4. Reproducible adversarial tests

The test in tests/triangle_sphere_sweep_tests.cpp includes:

- A triangle with vertices moving vertically from z=+2 to z=-2
  through a unit sphere; genuine interior collision begins at
  t=.25. The certified prefix must stay before this instant.
- A stationary triangle well away from the sphere; the entire
  time interval must be certified clear.
- A huge triangle with all three vertices outside the sphere
  while its interior already intersects at t=0; the
  certified-safe prefix must be zero.
- 640 seeded randomized continuous 3D triangle paths, with
  201 independent sample checks each (128,640 total).
- Reproduction of the actual integrated FEM's FIRST step
  where the existing vertex-only response leaves a
  triangle penetrating the sphere, using the real 180-node
  480-tetrahedral dynamics, not an invented geometry.

Finite sampling helps detect coding mistakes; it is NOT the
mathematical proof. The Lipschitz inequality provides the
one-sided exact-arithmetic argument.

## 5. What this does NOT yet achieve

The new certificate is a narrow-phase geometric gate. It is not
yet wired into the physical solver's accepted timestep selection.
As of this pass, the original vertex-only sphere response is
preserved without silently changing older signed-off receipts.

To make the SOLVER surface-safe, use a globally synchronized,
transactional time-of-impact step. A correct future design needs
to:

- combine the minimum safe prefix across ALL faces, not move
  individual vertex faces with inconsistent time clocks;
- advance the whole body no farther than the earliest certified
  event while preserving positive tetrahedra;
- identify the nearest triangle point and its barycentric
  participation at the collision;
- apply physically justified generalized contact impulses or a
  constrained/barrier solve, not just a naive positional nudge;
- re-integrate the leftover timestep with updated forces and
  velocities, maintaining deterministic termination at resting
  and grazing contacts;
- enforce no initial penetrations and explicitly handle
  friction, repeated contact and self-contact;
- independently audit BOTH all accepted triangle states and
  the complete swept triangle path on every accepted time
  substep; test actual solver energy/momentum/cost;
- compare against official IPC Toolkit feature-level CCD
  on triangulated nonplanar obstacles with equivalent
  contact/thickness semantics.

Until that is accomplished, the only accurate claim is
**conservative whole-triangle collision *detection* capability**,
not a generally contact-safe deformable solver.

## 6. Build and verify

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build -R \
      'triangle_sphere|continuous_whole_triangle' \
      --output-on-failure

Native CI across Ubuntu/macOS Debug/Release:
https://github.com/swayam8624/kyoto-common-contact/actions

All actual FEM source data, six plots, 181 volume states and
901 independent surface audit records remain in the
Pass-6 raw evidence artifact. Do not edit those signed-off
source measurements to make a plot look physically correct.

This is a genuine mathematical and implementation improvement
toward SCA-safe, high-contact nonplanar solver experiments.
No speedup over official IPC LBVH or algorithmic novelty is
asserted.
