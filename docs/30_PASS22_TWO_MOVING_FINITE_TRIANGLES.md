# Pass 22A — Conservative continuous separation for TWO moving finite triangles

## The missing geometric prerequisite

The original Kyoto static-rigid finite-triangle collision solver had a
time-consistent accepted-substep integrator and a conservative test of
**one moving triangle against one static finite triangle**.

A genuinely coupled deformable/deformable solver needs both surfaces to
move during the same accepted physical step. Treating the other body
as static is unsafe: a second moving face can cross the first face's
trajectory even when its old position was far from the first motion
envelope.

`include/kyoto/moving_triangle_pair_certificate.hpp` now exposes

`solid::certify_two_moving_triangles(a_motion,b_motion,skin,max_depth)`

where both inputs are explicit LINEAR vertex trajectories, each with
three 3D start and end points. This is a pure, read-only geometric
predicate. It must never commit source velocities, positions, or
physical elapsed time.

## Why the relative-motion guard is conservative

For a linearly moving triangle `A(t)`, any point on `A(t)`
corresponds to the same barycentric point on `A(mid)`. Over the
subinterval `[t0,t1]`, the norm of their separation is bounded by

```text
max_dist(A(t), A(mid))
   <= 0.5 * max_i ||A_i(t1) - A_i(t0)|| .
```

The same bound holds for the independent triangle `B`.

By the triangle inequality for distances between compact sets,

```text
d(A(t),B(t))
  >= d(A(mid),B(mid))
       - 0.5 * max_i ||A_i(t1)-A_i(t0)||
       - 0.5 * max_j ||B_j(t1)-B_j(t0)|| .
```

When this lower bound, reduced by the conservative floating-point
guard, exceeds the requested contact thickness, **the entire
subinterval is separated in the arithmetic model**.

A second sufficient test uses both full swept spatial AABBs. If
those boxes are separated on an axis by more than the contact
thickness and guard, their finite triangles cannot touch during
the interval. The routine recursively subdivides only unresolved
intervals. It NEVER returns "safe" after exhausting the time
subdivision budget. Instead the result is unresolved, and a
time-consistent contact solver must subdivide and *reintegrate* the
physical step or invoke an independently validated robust CCD oracle.

## Explicit limits

1. The formula is mathematically conservative for the **specified
   linear** vertex trajectories. It is not a prediction for
   unrecorded nonlinear/multi-substep physical trajectories.
2. The binary64 implementation has a scale-dependent guard but
   no formally directed rounding for all intermediate geometry
   predicates. Do not advertise universal floating-point certification.
3. "Unresolved" does **not** prove intersection or establish a
   contact-time-of-impact. It only prohibits a false claim of no contact.
4. Degenerate/invalid finite-element faces and general multi-body
   contact manifolds need separate robust treatment.
5. This geometric test does not implement force coupling,
   equal/opposite nodal contact impulses, friction, solver convergence,
   conservation, self-contact, or original solver-step timing.
6. Broadphase candidate validity and actual CCD events are separate
   invariants. Do not substitute one for the other.

## Independent tests and verification

`tests/moving_moving_triangle_certificate_tests.cpp` checks a
moving/static limiting case against the earlier static-target
certificate, actual moving/moving head-on crossing, near-miss,
coincident start, symmetry under exchanging the two objects,
nonfinite input rejection, and 120 seeded geometric stress scenes.

Dense time sampling is an independent *counterexample finder*, not a
complete CCD algorithm. A sampled contact that the certificate
pruned is a fatal error; a truly separated scene that the certificate
left unresolved is an allowed conservative false positive.

The dedicated
`.github/workflows/pass22-moving-moving-ccd.yml`
builds and runs the same C++23 tests on Ubuntu and macOS.

### Test locally

```bash
cmake -S . -B build-pass22 -DCMAKE_BUILD_TYPE=Release
cmake --build build-pass22 --parallel 4 \
  --target kyoto_moving_moving_triangle_certificate_tests
ctest --test-dir build-pass22 --output-on-failure \
  -R conservative_moving_moving_finite_triangle_time_guard
```

## Research status

This is **foundational geometric engineering**, not a stand-alone
novelty claim. Moving/moving continuous collision detection,
time subdivision, Lipschitz bounds, and conservative contact
certificates are established topics. The next physically meaningful
test must use solver-produced moving-moving accepted leaf endpoints,
a genuinely coupled opposite-impulse collision response,
independent momentum and mechanical-energy audits, and an official
upstream IPC CCD event oracle.
