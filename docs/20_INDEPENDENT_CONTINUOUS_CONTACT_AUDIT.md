# Pass 9 — Independent swept geometry and contact-impulse identities

**Central question:** Pass 8 uses a midpoint-distance/Lipschitz inequality
to accept physical FEM time intervals. Does a genuinely different continuous
geometric argument agree? Separately, does the solver's local barycentric
contact impulse obey independently derived conservation/work identities?

This pass builds **two independent audit paths**. Neither changes the
accepted physical trajectory. The geometric path measures the continuous
swept triangle, not only its endpoints. The impulse path measures changes
in actual three-dimensional mass-weighted velocities, not only the
impulse magnitude reported by the implementation under test.

The original Pass-8 solver is unchanged. No algorithmic novelty,
whole-solver acceleration, general mesh contact solver, self-contact
correctness, full energy-conserving elastodynamics or robust floating-point
predicate proof is claimed.

## 1. Geometry: the six-endpoint convex-hull theorem

The moving surface triangle has three vertex paths, each assumed
LINEAR over the same accepted physical substep:

    p_i(s) = (1-s) a_i + s b_i, i in {0,1,2}, s in [0,1].

Any point q(s) inside the moving triangle has nonnegative
barycentric weights w_i summing to one:

    q(s) = sum_i w_i p_i(s)
         = sum_i (1-s) w_i a_i + sum_i s w_i b_i.

All six coefficients in the last expression are nonnegative and
sum to one. Therefore, for every time s and every point of the
triangle,

    q(s) in conv{a0,a1,a2,b0,b1,b2} = H.

This *swept prism enclosure* is geometrically independent from
Pass 6B's midpoint-distance Lipschitz argument.

Consider a static sphere of center c and radius r.
If we can find ANY nonzero separating direction n such that

    min_{v in {a0,a1,a2,b0,b1,b2}} n·(v-c) > r ||n||,

then by linearity every q in H satisfies

    n·(q-c) > r ||n||.

Cauchy–Schwarz then guarantees

    ||q-c|| >= n·(q-c)/||n|| > r,

so the ENTIRE swept triangle, at ALL intermediate times, is
strictly outside the rigid sphere.

This is a one-sided **continuous** clearance certificate in
exact arithmetic. Merely checking triangle start/end distances
would not suffice.

### How a candidate separation plane is found

The implementation in
include/kyoto/independent_prism_audit.hpp explores:

- positive/negative world coordinate axes;
- six vectors pointing from the center to each prism vertex;
- both start/end triangle normals and their opposites;
- an independent iterative convex-hull support search.

The last procedure starts at the nearest endpoint vertex in
Euclidean norm, then repeatedly finds the hull endpoint minimizing
the current dot-product direction and moves toward it with a
clamped closest-line coefficient.

The iterative search is **only a method of proposing a plane**.
Each candidate is independently checked against all six endpoint
projections. A failed heuristic cannot authorize marking an
interval safe.

If no strict plane is found, bisect the time interval and
try again on each smaller six-endpoint hull. At the configured
depth limit, record **UNRESOLVED**; do not silently return safe
or infer that an actual collision necessarily happened.

This is particularly important for true tangencies, almost
grazing events and globally nonconvex swept paths whose
six-point convex hull intersects the sphere even though
the actual moving triangle does not.

### Floating-point scope

The inequality is exact over real numbers. The present
implementation evaluates projection, norm and convex-hull
support directions in ordinary IEEE binary64. A scale-aware
strict positive margin deliberately reduces numerical overclaim,
but is NOT a formal directed-rounded interval guarantee.
Large coordinate shifts, underflow and near-tangent predicates
need a more rigorous exact/directed-rounded follow-up.

The valid conclusion is:

    witness FOUND: exact-arithmetic full-interval separation;
    witness NOT FOUND: unresolved; may be clear or colliding.

Do not claim the implementation is machine-certified merely
because all finite tests pass.

## 2. An independently programmed static triangle distance

Pass 6 used closest_triangle_point(), implemented by testing
the plane projection, vertices and three closed edges.

Pass 9 implements a *different* algorithm in
include/kyoto/independent_triangle_distance.hpp:

- classify the closest point into one of seven Voronoi regions
  (three vertices, three segments and the interior);
- compute the appropriate barycentric coordinates analytically
  in that region;
- handle geometrically degenerate triangles by a separate
  clamped-three-segment fallback;
- never call the production closest_triangle_point() routine.

The two computed distances are compared in 1,000 seeded
randomized moving-triangle fixtures, all 900 native FEM steps
and every leaf of the fast adapted impact.

Small finite-test discrepancy means independent algorithms agree
on those inputs. It is NOT independent exact arithmetic or a
full geometrical proof under every floating-point mode.

## 3. Applying the independent continuous audit to actual FEM

Run the unmodified Pass-8 time-consistent physical integrator
for 900 true steps, with 180 moving vertices, 320 connected
boundary triangles and 480 volumetric neo-Hookean tetrahedra.

For each macro-step:

1. Obtain the originally accepted physical substep durations.
2. Re-simulate each actual accepted substep from its immediately
   preceding accepted state. Check it was not clipped, and the
   reconstructed final body agrees with the original solver.
3. For each of the 320 genuinely moving triangular faces,
   compare BOTH endpoint distances with the new Voronoi oracle.
4. Apply the independent six-endpoint swept-prism test across the
   COMPLETE original physical interval, not just visual frames.
5. Record how many independent continuous certificates were
   established and how many intervals remain unresolved.
6. Repeat the test for the near-contact 50 m/s physical event,
   preserving every accepted smaller time interval and each
   triangle's source centroid and proof status.
7. For unresolved intervals, sample 33 genuine triangle positions
   as a **regression**, without calling such sampling a proof
   of continuous safety.

The source CSVs record actual paid separating-plane evaluations,
time subdivisions, independent static clearances and differences
between the two genuinely distinct finite-triangle distance
implementations.

An unresolved interval must be included in every summary;
a passing finite static audit does NOT erase it.

This experiment is specific to a rigid ANALYTIC sphere and linear
substep interpolation. It is not a certificate for general moving
triangle/triangle CCD, cubic deforming paths or thick/contact-barrier
semantics used in IPC Toolkit.

## 4. Physics: the exact local barycentric impulse identities

Pass 7's face impulse distributes one rigid-contact impulse
through the actual triangle's nearest-point barycentric weights.

Let contact point q=sum_i w_i x_i; its relative velocity to
the static sphere is u=sum_i w_i v_i. With positive nodal masses
m_i, define the effective inverse contact mass:

    k = sum_i (w_i^2/m_i).

If a single contact impulse vector J is applied to the body
through the triangle, each vertex velocity changes by

    delta_v_i = (w_i/m_i) J.

Then independent mass-weighted linear momentum must change by

    delta_P = sum_i m_i delta_v_i
            = sum_i w_i J = J.

Angular momentum about a fixed world origin must change by

    delta_L = sum_i x_i cross (m_i delta_v_i)
            = (sum_i w_i x_i) cross J
            = q cross J.

This identity works under arbitrary translations and rotations,
not merely a contact aligned with the z axis.

For the frozen positions during the instantaneous impulse, the
kinetic energy change is **exactly**

    delta_K = sum_i m_i (v_i·delta_v_i + .5||delta_v_i||^2)
            = u·J + (k/2)||J||^2.

Decompose contact velocity u into incoming normal component
u_n=u·n<0 and tangent component u_t=u-u_n n.

The implementation's normal impulse is

    J_n = -(1+e) u_n/k,    0 <= e <=1.

Friction impulse magnitude is limited by

    J_t <= min(mu J_n, ||u_t||/k),

acting opposite the tangential velocity. Therefore:

    delta_K_normal = -(1-e^2) u_n^2/(2k) <= 0

    delta_K_tangent = -J_t||u_t|| + .5 k J_t^2 <= 0.

Hence one isolated contact impulse cannot increase kinetic
energy in exact arithmetic under these settings.

### The new randomized experiment

tests/contact_impulse_balance_tests.cpp rotates and translates
the entire nondegenerate face and sphere through 1,200 seeded
three-dimensional configurations with unequal vertex masses
and independently varied incoming velocities, restitution and
friction coefficients.

It computes full kinetic energy, total linear and angular
momentum directly from the BEFORE and AFTER velocity fields,
independently derives the expected changes, and checks the
body's actual impulses and their barycentric shares.

The native CSV includes all predicted and measured energy
changes and momentum residuals. It includes perfectly elastic
normal cases (e=1, mu=0) as specific edge tests.

**Scope limitation:** this is a LOCAL, instantaneous impulse
identity with positions held fixed. The complete explicit
neo-Hookean FEM also integrates elastic force, gravity, velocity
damping, finite contact activation and time discretization.
Total mechanical energy is not generally conserved in that model.
No local identity automatically validates the global simulation's
material accuracy or its contact response against experimental
physical truth.

## 5. Seven scientific output figures

scripts/render_pass9_independent_audit.py creates:

1. Independent continuous certification *coverage* across all
   physical timesteps, with unresolved intervals shown separately.
2. Paid plane-test computational effort for routine and fast paths.
3. Native-vs-independent static triangle distance discrepancies.
4. Measured vs analytical single-contact kinetic work, 1,200 cases.
5. Full 3D linear and angular momentum balance residuals.
6. Observed isolated-contact energy loss against restitution
   and friction variations.
7. The original true Pass-8 deformable FEM 3D mesh, computed stress,
   actual rigid sphere and independent swept-proof status of
   each of its 320 connected faces.

The interactive 3D figures from Pass 8 remain in the previous
archive. No positions, stress, timings or impulse measurements
are artistically invented by the Pass-9 renderer.

## 6. Build and reproduce

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

    ./build/kyoto_independent_contact_tests results/pass9-independent
    ./build/kyoto_impulse_balance_tests results/pass9-independent

    python3 -m pip install numpy pandas matplotlib pillow
    python3 scripts/render_pass9_independent_audit.py \
      --input results/pass9-independent \
      --physics results/time-consistent-fem \
      --output results/pass9-independent/visuals

The renderer's --physics input is created by Pass 8:

    ./build/kyoto_time_consistent_experiment results/time-consistent-fem

Original numerical evidence, all seven charts and face-level
proof-status receipts are archived in the native GitHub Actions
workflow as
kyoto-stage9-independent-swept-CCD-and-contact-impulse-audit.

**Scientific gate:** do not infer pass/fail from source code being
committed. The complete CI run must succeed and original archive
must exist; unresolved intervals and numerical residuals must be
inspected from that archive before any claim of full continuous
coverage.

## 7. Actual original verified experimental receipts

The source-backed independent audit from 23 November 2026 is a
completed, five-job GitHub Actions success on Ubuntu/macOS in
Debug/Release, including original visualization/provenance checks:

https://github.com/swayam8624/kyoto-common-contact/actions/runs/45144892694

Native measured results from the artifact's own manifests:

| Native independent evidence | Verified result |
| --- | ---: |
| Routine FEM physical macro-steps | 900 |
| Routine complete swept face paths | 288,000 |
| Independently certified routine paths | 288,000 |
| Routine unresolved paths | **0** |
| Independent support-plane evaluations (routine) | 4,916,586 |
| Fast true physical substeps | 5 |
| Fast complete swept face paths | 1,600 |
| Independently certified fast paths | 1,600 |
| Fast unresolved paths | **0** |
| Minimum routine endpoint full-triangle clearance | 1.127650231 mm |
| Minimum fast endpoint full-triangle clearance | 0.868559517 mm |
| Maximum static distance-oracle disagreement (native FEM) | 2.220446049e-16 m |
| Rotated/transformed contact impulses independently tested | 1,200 |
| Maximum local kinetic-energy work residual | 6.366462912e-12 J |
| Maximum linear-momentum impulse residual | 1.705302566e-13 N·s |
| Maximum angular-momentum impulse residual | 9.374856803e-13 N·m·s |
| Maximum observed local positive kinetic increment | 5.456968211e-12 J |

The strict separating-plane check succeeded on the ORIGINAL six-point
hull of every routine interval: 288,000 routine time nodes for
288,000 moving triangles. Recursive temporal splitting was not
needed in this specific set of real solver trajectories.
The **4,916,586** support-plane projections are actual paid
computation but NOT measured milliseconds or comparative SCA speedup.

The two finite-triangle oracles were implemented using DIFFERENT
numerical constructions (Voronoi regions versus exhaustive edges
and orthogonal projection), providing stronger finite test evidence
than running the same implementation under a different function name.

The test suite also independently rejected actual triangle/sphere
penetration at temporal midpoints despite safe endpoints, huge
triangle interior penetrations despite safe vertices, and nearly
machine-tangent geometries. A "not separated" result is always
UNRESOLVED, not silently interpreted as a safe interval.

The complete original sources are preserved as:

    kyoto-stage9-independent-swept-CCD-and-contact-impulse-audit

including independent_manifest.json, impulse_manifest.json,
real_fem_independent_sweeps.csv (900 rows),
fast_impact_independent_sweeps.csv (5 physical leaves),
fast_impact_face_witnesses.csv (1,600 face proof statuses),
impulse_balance_cases.csv (1,200 actual impulse law cases),
all seven data-grounded figures, and their combined research atlas.

**Absolutely do not generalize the zero-unresolved outcome to every
floating-point input.** Neither the support-plane tests nor the
production midpoint certificate use formally directed rounding.
The convex-hull and impulse identities are exact in real
arithmetic; their floating-point implementation has only finite
validated coverage. The full damped/FEM mechanical-energy
conservation claim remains explicitly false.

## 8. Remaining important work



- Directed-rounded exact conservative interval arithmetic for both
  accepted and externally audited continuous geometry.
- An independent **general feature-level** mesh-mesh CCD solver,
  including true VF and EE events and self-contact.
- Actual moving nonplanar rigid triangle meshes, not just a
  closed analytic sphere or a kinematic replay.
- Full physically coupled contact/elastic complementarity or
  variational barrier solve with measured energy/momentum and
  mesh quality across a convergence study.
- A/A, paid benchmark, statistically evaluated per-scene
  SCA incremental broad phase against pinned official IPC
  Toolkit LBVH with equivalent detection/solver parameters.
- An independent implementation, prior-art comparison and
  honest novelty audit before any publishable speedup claim.

The current pass improves the independence and precision of
our **scientific validation**, not the core solver speed.
