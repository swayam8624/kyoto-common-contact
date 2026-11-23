# Pass 10 — Outward-enclosed binary64 geometry and the full FEM momentum ledger

**Status:** This pass changes the *independent verification*
mathematics and the source-backed physical accounting of the
original 3D FEM solver. The solver still uses the Pass-8 adaptive
whole-face response, and does not acquire arbitrary mesh collision,
self-contact, a coupled implicit Newton/KKT solver, a new SCA
broad phase, or a speedup claim.

The central change has two distinct components:

1. Independent swept triangle–sphere separating planes are tested
   against **outward-enclosed floating-point intervals** rather
   than just padded nominal double inequalities.
2. Every accepted physical FEM step is audited for its complete
   linear/angular momentum update, including elastic forces,
   gravity, damping, actual external contact impulses and the
   transport term caused by moving node positions.

These are physically different questions. A strong geometric
certificate does not automatically imply a correct material
or contact solver, and an accurate discrete momentum ledger does
not establish experimental physical accuracy.

## A. Geometric theorem and numerical interpretation

For a triangular face with original initial/end vertices
a_i and b_i (i=0,1,2), each linear vertex trajectory is

    p_i(t) = (1-t) a_i + t b_i,       0 <= t <= 1.

At any time t the triangle consists of barycentric combinations

    q(t) = sum_i w_i p_i(t),
    w_i >= 0, sum_i w_i = 1.

Consequently each q(t) is inside

    conv{a0,a1,a2,b0,b1,b2}.

Let the rigid sphere center be c and its radius be r.
For ANY nonzero real direction n, the strict inequalities

    n·(a_i-c) > r ||n||  for i=0,1,2
    n·(b_i-c) > r ||n||  for i=0,1,2

guarantee that the entire swept triangle remains strictly
outside the sphere throughout the full time interval.

This is because the linear projection of every convex-hull
point is strictly above r||n||. Cauchy–Schwarz gives the
corresponding strict Euclidean distance > r.

This is an *exact-real-arithmetic mathematical proof*.
Pass 9 tested the inequality with an ad hoc scale-dependent
binary64 slack. Pass 10 makes the numerical burden explicit.

## B. Outward binary64 interval operations

Source:
include/kyoto/rounded_prism_certificate.hpp

Represent an uncertain real scalar by the closed binary64
interval [lo,hi]. Every operation is expanded outward by
one adjacent representable double using std::nextafter:

    add([a,b],[c,d]) =
       [round_down(a+c), round_up(b+d)]

    subtract([a,b],[c,d]) =
       [round_down(a-d), round_up(b-c)].

For a product, evaluate the four endpoint products,
then outward-expand the least and greatest result.

For square, explicitly handle positive, negative and
zero-crossing intervals to avoid incorrect negative lower
bounds. For sqrt, clamp the lower argument to zero and
outward-expand both square-root bounds.

For a candidate plane direction n, treat its computed
binary64 components as *exact representable inputs to
the verification inequality*. We do NOT claim that n
is an exact mathematical optimizer, and its potentially
rounded construction cannot invalidate a certificate:
**any direction is valid if it passes the interval test**.

Evaluate for every of the six interval-enclosed face points P:

    D(P) = n_x*(P_x-c_x)
         + n_y*(P_y-c_y)
         + n_z*(P_z-c_z),

with interval subtraction, multiplication and addition.

Compute an upper bound on the sphere's support scalar

    R = r sqrt(n_x^2+n_y^2+n_z^2)

using outward-enclosed squares, sum, root and product.

Accept the candidate plane ONLY if

    lower(D(P_i)) > upper(R)

for **each** of the six points.

Every path failing any comparison is UNRESOLVED; it is
never promoted to safely separated. The strict lower
margin is recorded only for a successful six-point
witness.

### Why dyadic time subdivision is mathematically subtle

It would be incorrect to recursively use nominal
double-precision midpoint coordinates as if they were
the exact original trajectory at the middle of the
time interval.

For example, at t=0.5 a nominal floating-point midpoint

    m = fl((a+b)/2)

might differ from the exact real midpoint. Treating
the two smaller segments [a,m] and [m,b] as if their
union necessarily enclosed the ORIGINAL real line can
lose narrow slivers of possible motion.

Therefore Pass 10's recursive search keeps the ORIGINAL
representable a_i and b_i fixed, chooses exactly
representable dyadic t0/t1 and forms fresh interval
positions from

    a_i + t*(b_i-a_i)

via outward-enclosed operations.

These endpoint *intervals*, not nominal rounded
midpoints, are used in the actual six-plane test. The
candidate plane may be proposed from a rounded
interpolated point; this does not matter, because the
test validates it against the wider intervals.

At recursion depth <=22 the dyadic rational t is
exactly representable in binary64. There is no hidden
time-parameter gap in this specified linear-path model.

### Detecting unsafe numerical configurations

The verifier explicitly checks:

- IEC 60559 / IEEE-style binary64 support;
- radix 2, FE_TONEAREST rounding mode;
- the availability of gradual subnormal arithmetic;
- no __FAST_MATH__ compile setting;
- finite source positions, normals and intermediate
  intervals;
- bounded dyadic recursion depth.

The dedicated verifier target is compiled with

    -ffp-contract=off -fno-fast-math

under Clang or GCC so that a compiler cannot silently
fuse an intended product/add pair into one non-equivalent
rounding operation.

**Crucial scientific caveat:** C++ and compiler switches
cannot formally prove at runtime that every relevant
hardware instruction or library sqrt/nextafter conforms
under every possible FTZ/DAZ, subnormal, overflow, vector
mode, JIT or optimization configuration.

This implementation offers explicitly outward-bounded
arithmetic under the stated IEEE/rounding semantics. It
must not be described as universally certified exact
predicates, MPFR interval arithmetic, formally
verified hardware behavior, or a general-purpose
bulletproof industrial CCD implementation.

## C. Numerical adversarial fixtures

tests/rounded_prism_certificate_tests.cpp checks:

1. A whole triangle that crosses a sphere at midstep
   while both endpoint triangles are separated. It
   must NOT be certified safe.
2. A stationary triangle tangent to the sphere. A
   strict separation witness must NOT be returned.
3. A nominally separated triangle only one ULP
   above the tangent plane. It cannot be blindly
   declared safe due to naive rounding.
4. A clear separated static triangle, which must
   be proven under the normal floating-point mode.
5. Deliberately changing the active floating-point
   rounding to FE_UPWARD; the verifier must refuse
   to run before producing any certificate.
6. Independent interval arithmetic cancellation,
   mixed-sign multiplication and square-root bounds.
7. 500 seeded clearly separated, randomly moving
   three-dimensional triangles.
8. All 320 physical surface triangles at each of
   the 900 integrated neo-Hookean FEM steps.
9. All actual physical leaves of the 50 m/s close
   impact that causes genuine adaptive time replay.

For the FEM motion, the verifier accepts source
positions and durations but NEVER modifies the
solver's physical nodal state or contact impulses.
The source test re-integrates accepted substeps to
recover the exact geometric paths used by the solver.

Native paid measurements include every proposed
separating plane, endpoint interval dot check, time
subdivision and strict positive lower margin.

The result may be 100% or lower coverage; if any
interval cannot be strictly proven under the chosen
depth, it MUST be reported unresolved.

## D. The complete discrete 3D momentum ledger

Source:
include/kyoto/whole_face_contact_dynamics.hpp
tests/whole_fem_balance_tests.cpp

The old source reported the sum of scalar normal
impulse magnitudes but not their actual VECTOR
resultant on the body. This is inadequate for
auditing full-body linear or angular momentum.

Pass 10 adds, to the original accepted contact
response, explicit measurements of:

    J_contact_vector = sum of all full 3D face impulses;

    tau_contact_at_application =
       sum of q_contact cross J_contact;

    tau_contact_about_step_start =
       sum of q_old_with_same_face_weights cross J_contact.

They include BOTH impulses applied before the Euler
position trial and impulses applied at the accepted
post-motion contact geometry. Contact tangential
impulses are included in the VECTOR source totals.

The original physical force evaluation and contact
law are not changed by these diagnostic fields.

### Linear momentum identity

Let

    P_old = sum_i m_i v_i,
    F_i = f_elastic_i + m_i g,
    damp = exp(-material_damping*dt).

The semi-implicit solver computes pre-contact
damped Euler velocities:

    v_trial_i = damp * [v_i+dt*F_i/m_i].

The external rigid-sphere face impulses add total
vector J_contact. Consequently, for an accepted,
unclipped, original physical substep,

    P_new =
      damp*[P_old + dt*sum_i F_i] + J_contact.

The independent ledger recomputes both P_old and
P_new from actual full-body nodal masses and
velocities. It re-evaluates every original FEM
elastic force and the gravity term.

It then computes the difference between the
measured full-body linear momentum and the right
side, in N·s and normalized to the size of the
actual momentum/force/impulse terms.

### Internal elastic-force balance

Each finite tetrahedron contributes four nodal
elastic forces summing to zero by translational
invariance of the neo-Hookean energy.

The full-body net internal force therefore
obeys, in real arithmetic,

    sum_i f_elastic_i = 0.

Likewise, hyperelastic Cauchy stress symmetry
and rotational invariance imply

    sum_i x_i cross f_elastic_i = 0.

The independent ledger checks both these sums
and scales their residuals by the sum of the
absolute force/torque magnitudes across the
whole mesh, so that a physically large stress
state does not make a meaningless absolute
tolerance look small.

### Angular momentum requires the motion term

Define

    L = sum_i x_i cross (m_i v_i).

Let T_i = x_old_i cross F_i.
An incorrect audit might claim

    L_new = damp*(L_old+dt sum_i T_i)
            + tau_external,

ignoring the fact that the Euler step actually
moved the nodes while updating velocity.

The true discrete identity is

    L_new =
        damp*(L_old+dt sum_i x_old_i cross F_i)
        + sum_i x_old_i cross J_i
        + sum_i (x_new_i-x_old_i)
                     cross (m_i v_new_i).

The final term is **discrete angular transport**,
arising solely from the change in position used
when measuring the angular momentum after the
Euler step. It is recorded as a separate scalar
in the original per-substep CSV.

The ledger checks this exact discrete identity
at every accepted physical substep, including
the fast impact's real adaptive time subdivision.

This does not pretend that the solver conserves
a rigid body's angular momentum while a static
sphere applies external contact impulses.

## E. Mechanical energy is measured, not assumed constant

The actual physics includes:

    E_mechanical =
       sum_i 0.5 m_i ||v_i||^2
       + E_neoHookean_elastic(x)
       + sum_i m_i g z_i.

The gravity energy uses z=0 for the source
datum; only energy differences matter.

Damping, friction, finite activation standoff
contacts and semi-implicit numerical integration
can dissipate energy or introduce timestep error.
A correct linear/angular momentum ACCOUNTING
identity does NOT by itself prove that the
mechanical energy is constant or physically
calibrated.

Every actual leaf records original before/after
mechanical energy, its signed difference,
contact update count and the true normal/vector
impulses. The scientific plot shows both
cumulative and per-step energy change without
quietly removing nonconservative behavior.

## F. Original data, figures and reproduction

Build:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Generate the independent receipts:

    ./build/kyoto_rounded_prism_tests results/pass10-rounded
    ./build/kyoto_whole_fem_balance_tests results/pass10-rounded

The new charts additionally use original Pass-8
native source states, first generated with:

    ./build/kyoto_time_consistent_experiment results/time-consistent-fem

    python3 -m pip install numpy pandas matplotlib pillow
    python3 scripts/render_pass10_numerical_audit.py \
       --input results/pass10-rounded \
       --physics results/time-consistent-fem \
       --output results/pass10-rounded/visuals

Seven original source-backed figure topics:

1. Outward-interval continuous-certification coverage,
   including any unresolved paths.
2. Every paid endpoint interval-dot/plane proposal,
   explicitly not a comparable IPC/LBVH timing.
3. Strictly positive interval lower support margins.
4. Linear/angular whole-FEM momentum balance residuals
   for both normal and fast physical integrations.
5. The extra angular transport and internal elastic
   torque cancellation terms, computed from real nodes.
6. Mechanical energy change and actual contact updates
   across the entire physical 900-step simulation.
7. Genuine curved-sphere, fully deforming FEM 3D
   geometry, where actual stress and independently
   certified lower margins are mapped onto the SAME
   320 connected original boundary faces.

The combined academic atlas preserves all seven
figures. Every input position, energy, force,
impulse, time and numerical certificate comes
from original native C++ code and is archived
by the reproducibility CI. The visualization
script does not integrate or invent physics.

Raw sources:

- rounded_manifest.json
- rounded_routine_900.csv
- rounded_fast_substeps.csv
- rounded_fast_1600_faces.csv
- whole_fem_balance_manifest.json
- whole_FEM_accepted_substep_balance.csv

The complete workflow uploads the immutable
research artifact under:

    kyoto-stage10-IEEE-outward-CCD-and-full-FEM-momentum-evidence

Do not call this pass complete before the exact
commit's native Ubuntu/macOS Debug/Release and
source-visualization jobs succeed.

## G. Gates still OPEN

1. Formal directed-rounded/exact predicate and
   robust corner cases across all supported
   hardware/compilers, including pathological
   coordinates and actual self-contact.
2. General deformable triangle-mesh vs moving
   rigid *triangular* collider response with
   VF/EE feature-level CCD; current sphere test
   is not a general mesh-contact solver.
3. Independent physical accuracy, whole-step
   energy budgets and step-size convergence
   against an external reference solver, not
   only algebraic self-consistency.
4. True solver-grade self-contact, contact
   manifolds, narrow-phase physical barriers
   and a fully coupled material/contact
   optimization or complementary alternative.
5. The central hypothesis: a complete,
   maintained SCA swept-contact broadphase
   with conservative invalidation, correct
   fallback, robust uncertainty analysis
   and fully paid matched timing against
   pinned official IPC Toolkit 1.6.1 LBVH
   across held-out real 3D deformables.

This is a numerically stronger verification
milestone. It does NOT establish SCA performance
or publishable broadphase novelty.
