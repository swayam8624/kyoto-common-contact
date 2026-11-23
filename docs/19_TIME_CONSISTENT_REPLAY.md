# Pass 8 — Re-integrating the real elapsed time after whole-face CCD

**Research scope:** native C++23 physically integrated 3D tetrahedral
neo-Hookean FEM, with conservative whole-face rigid-sphere contact,
mass-weighted barycentric impulses, and **actual physical-time
subdivision**, not a truncated displacement presented as a full timestep.

The previous 900-step whole-face solver is preserved unchanged as the
geometric-safety baseline. The new solver removes one specific
time-accounting defect, but is NOT a globally implicit contact optimizer,
an independently certified robust floating-point CCD library, a
general triangle-mesh collider, or a self-contact solver.

## 1. Precisely what was wrong with Pass 7?

The old solver proposed a full semi-implicit Euler trial over dt:

    v* = exp(-gamma dt)*(v + dt*M^{-1}f(x))
    x* = x + dt*v*.

A strict triangle/sphere Lipschitz separation certificate could report
an earliest safe fraction alpha in [0,1]. The old response accepted

    x_new = x + alpha*(x*-x)

but retained v* and advanced the physics clock by **the full dt**,
even when alpha was strictly below one.

The accepted displacement thus represented only a fraction of the
trial path, while the velocity calculation and source time label
corresponded to full dt. Calling alpha*dt the "time integrated" would
also be incorrect: the velocity came from a full-dt force evaluation.

In normal 900-step sphere impact experiments, this defect did NOT
activate because the anticipatory barycentric velocity impulses
maintained clearance before the proposed trial. All 900 steps had
alpha=1. The old defect is exposed by a deliberately faster
collision in which a single full-dt trial must be clipped.

## 2. Adaptive trial rejection and genuine physical replay

New implementation: include/kyoto/time_consistent_contact.hpp.

For a requested macro interval [t,t+dt], attempt one complete
physical step through the existing whole-face FEM implementation.

A candidate is acceptable ONLY when it is safe over the **entire
proposed actual substep** (alpha==1), its triangular faces are
nonpenetrating, its tetrahedral Jacobians remain positive,
and all calculated physical state and material quantities are finite.

If the full-step candidate would clip the global displacement
(alpha<1), REJECT the entire candidate, including its computed
contact impulses and the clock increment. Instead split the
actual interval in half and attempt both halves **sequentially**.

    integrate(t,dt):
        candidate = copy(accepted_state)
        try one full FEM/whole-face step over dt

        if candidate is fully safe:
            commit candidate
            record physical elapsed dt
        else:
            integrate(t,dt/2)
            integrate(t+dt/2,dt/2)

The second half is always computed from the *accepted end state of the
first half*, with freshly evaluated neo-Hookean forces, stresses,
velocities and contact impulses. It is not a reused interpolated
trajectory or a post-hoc rescaled animation.

A recognized inverted-tetrahedron trial is also rejected and halved,
but malformed inputs, unrelated runtime errors and invalid source
states remain fatal. An inverted tetrahedron is **never committed**.

A finite maximum subdivision depth, minimum dt and accepted-substep
budget guarantee bounded attempted work. If the budget is exhausted,
raise an explicit error and leave the input body AND user-visible
receipt vector unchanged.

## 3. Exact elapsed-time accounting

If an accepted macro-step has N leaf substeps of durations

    delta_t_1,...,delta_t_N > 0,

the method checks that

    sum_j delta_t_j = dt

within stated floating tolerances. It also checks consecutive
physical endpoints and records every accepted dt separately.

Only then may the final public physical clock be set to
t_initial+dt to eliminate summation drift.

The macro-step does NOT claim that time has elapsed merely
because an unsafe full-dt candidate was computed. Rejecting
a candidate never leaks its velocity, position, impulse or time.

Per-substep original evidence includes:
- start_time_s, dt_s, end_time_s;
- certified full-triangle clearance and min tetrahedral det(F);
- mass-weighted normal/tangential impulse;
- accepted CCD interval test count, face contact update count;
- bisection depth.

At macro-step level the method additionally records accepted
leaf count, rejected clipped and inverted proposals, deepest
subdivision, integrated dt, contact impulse and min clearance.
For comparability, rejected trial work is counted as *paid work*;
this implementation does NOT claim an efficiency speedup.

## 4. The controlled experiments

**A. Free-flight equivalence:** put the rigid sphere far away.
For 40 native FEM steps, the new adaptive method must accept
exactly one full substep each and agree with the original uncut
semi-implicit solver down to the imposed numeric tolerance.

**B. Normal 900-step 3D FEM impact:** use the existing connected
toroidal solid, stationary sphere, mass, elastic material,
gravitational forcing and dt=.00075 s. For every step, independently
audit its entire connected triangular surface and positive tet J.
The final physical time must equal .675 s. The reference case is
not expected to need global subdivision; the face-aware impulse
response already prevents contact penetration at this speed.

**C. A genuinely clipped physical event:** the same solid begins
fully separated from a sphere, but the sphere is positioned so its
nearest surface is precisely 0.015 m away. Every elastic vertex
starts with a downward velocity of 50 m/s and is advanced over
dt=.00075 s. The old code must return alpha<1, demonstrating
the missing-time bug. The new method must reject the proposed
full step, integrate more than one actual smaller physical
substep, maintain whole-face clearance and positive tetrahedra,
and account for the COMPLETE requested dt.

The scenario is deliberately much gentler than an original
900 m/s collision stress trial that inverted physical elements
and exceeded the chosen adaptive budget. That rejected result
is a meaningful model stability limit and should NOT be
silently reported as a successfully solved high-velocity case.

**D. Transactional rollback:** configure zero allowed
subdivision depth on the same clipped fixture. The solver must
fail and preserve every original node position, velocity,
physical timestamp and pre-existing external receipt.

**E. Numerical resolution comparison:** execute the SAME
material/contact solver with 256 finer timesteps over the same
fast event. Compare position and velocity differences against
both the old clipped result and new adaptive full-time result.
A fine version of the SAME solver is a temporal resolution
reference, **NOT independent physically correct ground truth**.

This test is a step towards convergence studies; a true
convergence claim additionally requires several decreasing
dt values, independent implementation checks and observed
error orders/limits.

## 5. Reproduction and honest scientific plots

Code:
- include/kyoto/time_consistent_contact.hpp
- tests/time_consistent_contact_tests.cpp
- apps/time_consistent_experiment.cpp
- scripts/render_time_consistent_lab.py

Reproduce locally:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

    ./build/kyoto_time_consistent_experiment results/time-consistent-fem

    python3 -m pip install numpy pandas matplotlib pillow
    python3 scripts/render_time_consistent_lab.py \
       --input results/time-consistent-fem \
       --output results/time-consistent-fem/visuals

Native source data:
- routine_900_physics.csv — 901 original physical states,
  energies, body center, all step receipts and certified
  surface separation.
- fast_adaptive_accepted_substeps.csv — every real accepted
  substep with elapsed time, contact impulse and clearance.
- fast_original_vs_adaptive_vs_fine.csv — original clipped
  displacement, new full-time solver and same-model 256-step
  numerical control; the old "verified integrated time" cell
  is intentionally NaN because it does not faithfully
  correspond to its claimed full-dt integration.
- fast_event_true_3d_states.csv — all original vertices,
  velocities, actual computed von Mises stress and
  displacement at each of four distinct actual solver states.
- fast_event_surface_faces.csv — original connected mesh
  topology; 3D images must preserve this connectivity.
- manifest.json — actual configured geometry, physical dt,
  bisection counts and limits.

Five source-backed figures visualize:
1. Old nominal partial-motion prefix vs the new time substeps
   that ACTUALLY add up to requested dt.
2. Contact impulse and positive whole-triangle clearance on
   each accepted substep.
3. Distances from the same-model 256-step numerical reference,
   with the independence caveat clearly stated.
4. Genuine 900-step kinetic, elastic and gravitational energy.
5. Four real 3D FEM states (initial, old, adaptive, fine)
   with native stress and the actual static sphere.

All values and geometry originate from the native C++ solver.
No script fabricates or re-integrates a trajectory.

The five GitHub workflow jobs cover Ubuntu/macOS in Debug/Release
and an independent raw-source visualization job. Always use
the original passing Action run as the actual verification
receipt; code committed to GitHub alone is not a passing test.

## 6. What remains for scientifically robust contact

Time accounting is now honest for **successfully accepted**
adaptive macro-steps. It does not establish full physical fidelity:

- The 50 m/s test is a finite example, not general extreme
  high-speed stability; the failed 900 m/s example remains.
- The collision still uses a finite standoff shell and
  sequential Gauss–Seidel local impulses, not an implicit
  coupled contact/elastic optimization.
- The sphere is a smooth stationary analytic obstacle,
  not an arbitrary deforming triangle mesh.
- Self-contact is not resolved, and numerical FP interval
  certification requires directed rounding.
- Fractional time replay costs repeated force, geometry and
  time-slab work. No SCA efficiency improvement is claimed.
- Energy and momentum fidelity must be compared with
  independent solvers and systematic convergence.
- General external IPC Toolkit narrow-phase and rigid mesh
  contact with equivalent thickness remains necessary.

The next gate is a coupled physically time-consistent contact
response with rigorous numerical CCD and held-out complex
mesh scenes, followed by a fully paid baseline study against
actual pinned official IPC LBVH.
