# Stage 1 — Mathematical motion enclosure, falsification, and 3D atlas

**Status (23 November 2026):** implementation + tests + kinematic
visualization. This is not a solver integration or an algorithmic
performance result.

## 1. Why an endpoint-box is not a swept bound

A broad phase must conservatively produce ALL candidates that may
become relevant during the solver's entire declared motion interval.

For vertex k on moving triangle i, let its position be the cubic Bézier

    x_i,k(t) =
      (1-t)^3 P0 + 3(1-t)^2 t P1
      + 3(1-t)t^2 P2 + t^3 P3,       0 <= t <= 1.

The four Bernstein coefficients are nonnegative on this interval and
sum to one. Therefore every x_i,k(t) belongs to the convex hull of its
four control points. Consequently, for EACH spatial axis a,

    min_j (Pj)_a <= (x_i,k(t))_a <= max_j (Pj)_a.

The union of ALL 12 control points for three triangle vertices has an
AABB containing every triangle vertex at every continuous time.
Every position of the triangle is the convex combination of its three
vertices; thus the ENTIRE triangle is also enclosed.

Add thickness h>=0 by Minkowski addition with [-h,h]^3:

    S_i = AABB({P_i,k,j : k=0,1,2 and j=0,1,2,3})
          plus [-h_i,h_i]^3.

This is a sufficient swept candidate bound. It can be very loose.
It is NOT the exact spacetime occupied volume. True continuous
collision events require the narrow phase.

A pair of conservative swept boxes may overlap when no true continuous
contact occurs. False positives are acceptable and their cost is
measured. False negatives are unacceptable under the declared contract.

## 2. A rigorous conceptual counterexample, with known contact time

Use three vertices of the same translated triangle and define their
common longitudinal displacement:

    x(t) = 3A t(1-t).

Equivalently, the Bézier controls are P0=0, P1=A, P2=A, P3=0.

For A=4:

    x(0) = x(1) = 0,
    x(1/2) = 3,
    max_{0<=t<=1} x(t) = 3.

Put a stationary congruent triangle at x=3, with the same
two transverse coordinates. At exactly t=1/2, all three moving
vertices coincide with the stationary triangle's vertices.

Endpoint-only broad phase, even with thin positive thickness, sees
disjoint boxes at x≈0 and x≈3. It REJECTS this true event.

Complete cubic-control hull is [0,A] along X, inflated for thickness;
it INCLUDES x=3. The independent exhaustive and SAP oracles agree
that the pair (11,19) must be emitted.

This demonstrates failure of an unjustified shortcut. It does NOT
demonstrate a fast SCA algorithm or robustness of general triangle CCD.

## 3. Analytical phase diagram and exact limitations

For positive A, the trajectory reaches every x in [0,3A/4].
With a stationary target X>=0, neglecting thickness in the diagram:

    0 <= X <= 3A/4   actual exact pointwise contact at some t
    3A/4 < X <= A    control-box candidate but NO contact
    X > A           no control-box candidate

These are analytical classifications for the *special congruent
triangle family* and identical transverse coordinates. They do not
transfer to arbitrary intersecting triangles.

The intersection at X=3A/4 is a touching/tangency event at t=1/2.
This boundary case intentionally stresses numerical collision tests.

## 4. What the code can and cannot certify

The native implementation:
- validates time in [0,1], finite control coordinates and nonnegative,
  finite thickness; rejects overflowed bounding-box expansion;
- forms a conservative full-control-point hull, inflated outward by
  one representable floating-point step on each face;
- adapts the resulting box to the SAME shared primitive candidate API;
- checks equality of exhaustive and sweep-and-prune pair lists;
- tests 350 deterministic randomized cubic triangle sets at 102 sampled
  times (3 vertices) plus the analytical counterexample;
- exports all 121 native analytic trajectory frames, source control
  points, both AABBs, oracle pair counts and JSON scope metadata.

The mathematical convex-hull proof is exact for *real-valued cubic
paths defined by those control points*. The floating-point
implementation performs ordinary double arithmetic with one-ULP
face expansion and has NOT been machine-proved to enclose arbitrary
nonlinear solver paths or account for all FTZ/DAZ/compiler/FP-mode
possibilities. Rigorous directed-rounding interval arithmetic and
independent numerical audits remain required for a high-assurance CCD
or on-the-fly adaptive subinterval certificate.

The random sample tests help detect implementation mistakes, not prove
all-time containment. The mathematical proof supplies the guarantee
in exact arithmetic under its assumptions.

## 5. Which visual fields are actually grounded

1. 3D swept-envelope comparison: C++ exported moving vertices and
   certified/incorrect box faces, with actual coincident geometry.
2. Time-position comparison: C++ exported positions and exact target.
3. Spacetime (x,y,t) embedding: exported vertices over normalized time;
   Z spatial coordinate is projected out, clearly labelled.
4. 2D phase/regime contour: exact analytic classification of the
   cubic family, not measured performance or solver stress.
5. Offline Plotly interactive 3D: 121 native exported geometry frames.
6. ParaView VTK/PVD: 121 native geometry frames with a scalar called
   longitudinal_displacement (not stress, pressure or strain).
7. Optional short GIF from native exported frames, not synthetic
   simulation or solver footage.

Every figure and frame is derived from the C++ producer's CSV rather
than an independent decorative animation formula.

## 6. Reproduce

From repo root (macOS/Linux):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

    ./build/kyoto_motion_export results/motion-lab
    python3 -m pip install numpy matplotlib plotly
    python3 scripts/render_motion_lab.py \
      --data results/motion-lab \
      --output results/motion-lab/visuals
    python3 scripts/export_paraview_motion.py \
      --data results/motion-lab \
      --output results/motion-lab/vtk

Open the standalone offline Plotly HTML, or open
results/motion-lab/vtk/motion.pvd in ParaView.

The stdout and raw manifest are the first-stage evidence. CI should
also archive an immutable data+figure bundle and source revision.

## 7. What comes next — not yet implemented

The real scientific baseline is IPC Toolkit (official LBVH + independent
CCD), with matched trajectory, proximity, topology and feature
semantics. We then instrument an actual solver with contact-rich mixed
object scenes, build a baseline scaling suite, and only then implement
the SCA local frontier / refit / rebuild experiment. Nothing here
constitutes proof of a fast or novel common broad phase.

## 8. Prior art (must be credited)

- IPC Toolkit official LBVH and CCD: https://github.com/ipc-sim/ipc-toolkit
- Fast and Exact Continuous Collision Detection with Bernstein Sign
  Classification (2014): https://pmc.ncbi.nlm.nih.gov/articles/PMC4283478/
- A Large-scale Benchmark and an Inclusion-based Algorithm for CCD:
  https://doi.org/10.1145/3460775
- ParaView VTK Python visualization and time series:
  https://docs.paraview.org/en/latest/UsersGuide/displayingData.html

Bernstein convex hulls have long-standing published use in CCD.
We make NO claim that this Stage-1 mathematical argument is novel.
