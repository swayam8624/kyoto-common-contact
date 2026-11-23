# Instructions for every agent working on Kyoto / Spacetime Contact Atlas

The user requested a CLEAN RESEARCH RESET. Read README and docs/01 through
docs/22 before modifying source. Do not restore V4/RBC/CBP or old slide
pipelines. Existing historic branches remain untouched.

The only active thesis is conservative, fully paid swept-contact candidate
generation for moving deformable triangle-surface scenes, with localized
contact. Physics correctness comes first, not beautiful animation.

## Non-negotiable implementation rules

- C++23 core. Python may analyze benchmark data, never invent or modify
  measured run receipts. SDL3/Metal is the planned macOS display backend.
- Separate exact swept AABB pair parity from true CCD / solver contact.
- Valid input bounds and topology epochs, stable IDs, self-contact safety,
  mask handling, numeric boundary checks, and explicit safe fallback.
- A heuristic cannot authorize dropping a candidate. No stale certificates.
- Independent native oracle, official IPC Toolkit LBVH and continuous
  narrow-phase adapter are required before positive research claims.
- A mesh's deformation must be solver-generated. Equal candidate sets must
  not be depicted as different physical folds.
- In Pass 8, a clipped or inverted full-dt proposal is REJECTED
  and the *real* physical timestep is bisected and re-integrated.
  Only accepted full substep durations count toward elapsed time;
  aborting a macro step must preserve the original state and receipts.
- The 50 m/s, 15-mm-gap fixture tests nontrivial subdivision. A
  prior 900 m/s shock case exceeded the finite temporal budget
  after tet inversion. Retain that negative result instead of
  asserting that adaptive substepping solves arbitrary impact speeds.
- A 256-substep SAME-model numerical comparison is a refinement
  reference, never independent validation of physical fidelity.
  Legacy clipped alpha*dt is a NOMINAL displacement fraction,
  not physically integrated elapsed dt.

- In Pass 7A, the routine 900-step whole-face FEM has 0/901 accepted
  penetrating faces, versus 458/901 in the older vertex-only control.
  Global clipping was NOT activated on the routine run. Do not attribute
  all of that win to the time certificate; the face-aware mass-weighted
  contact impulses supplied the active safety response.
- The adversarial 900 m/s test exercises the whole-face time certificate
  separately. Fixed-clock global alpha<1 is a conservative geometric
  expedient, NOT a time-consistent contact integrator. Preserve physical
  energy differences and 901-state source audits.

- Explicitly audit full triangle interiors against curved rigid obstacles:
  zero penetrating vertices does NOT imply safe triangle surfaces.
  Pass 6 found 458/901 penetrating-face states despite zero vertex failures.
- Pass 7 and Pass 8 apply the midpoint-Lipschitz whole-face
  certificate to restrict accepted physical motion. Contact RESPONSE
  is a distinct, local barycentric impulse approximation; do not
  conflate geometric detection with globally coupled IPC physics.
- Pass 9 independently validates ALL 288,000 routine and 1,600
  fast-impact moving triangle/sphere intervals using a SIX-ENDPOINT
  swept-hull separating-plane witness, not the original solver
  midpoint-distance certificate. The 0 unresolved result is true
  ONLY for those finite source trajectories; not machine-directed
  rounding or arbitrary mesh-mesh CCD.
- Pass 9's 1,200 3D local impulse energy/momentum identities refer
  to FROZEN-POSITION face impulses. Do not assert total conservation
  of kinetic+elastic+gravitational energy in the damping/friction
  and finite-dt neo-Hookean integrator.
- Pass 10's IEEE interval verifier uses explicit outward bounds on
  all endpoint dot/norm operations, plus exact-dyadic interval
  positions from ORIGINAL physical vertex endpoints. It is strict
  only under stated IEC559/nearest/subnormal/no-fast-math semantics;
  do NOT claim formal universal hardware/compilation proof.
- Pass 10 verified all 288,000+1,600 accepted physical moving
  triangle/sphere face paths in its supported arithmetic model,
  0 unresolved on those particular source trajectories. This is
  NOT general deformable triangle-mesh / triangle-mesh CCD.
- Whole-body discrete P/L accounting now measures actual external
  3D contact vector impulses plus elastic forces, gravity, damping
  and Euler angular position-transport. It is NOT full physical
  energy conservation or calibration. Preserve original routine
  -459.624 J and fast -14545.237 J energy results and scientific
  concern about extreme dissipative contact.
- Pass 11 has true, nonanalytical, finite triangulated rigid
  contact (49 static rigid vertices/72 OPEN two-sided finite faces)
  with original 180-node/480-tet source FEM. In the source 900-step
  reference, 517 physical steps carried real contact and 11,731
  original barycentric face impulse updates; accepted full-surface
  gap stayed positive, min tet J 0.72287. Routine did NOT require
  temporal subdivision.
- The ACTUAL separate 20 m/s close impact REQUIRES rejected unsafe
  finite-triangle proposals. Evidence: two rejected, THREE
  physical time substeps accepted, the whole .750ms integrated.
  Verify the raw manifest and leaf receipts on every refactor.
- Earlier sphere-only dynamics do not respond to the finite rigid
  patch; when audited against the SAME 72 triangles they actually
  intersect in 95/181 sampled source states. This is a negative
  geometry/physics control, not a matched runtime comparison.
- Pass11's moving finite triangle vs static finite triangle
  Lipschitz proof is exact-arithmetic; its binary64 guard is NOT
  a directed-rounded general IPC VF/EE predicate. The static
  patch is open/two-sided, not a closed obstacle, moving rigid
  response, self-contact, or implicit KKT solver.
- Pass11 original 3D visualizations depict physical FEM frames,
  computed stress and 72 original finite rigid faces. Never
  fabricate geometry or portray sphere-only and finite-patch
  trajectories as equal-physics competing algorithms.
- The central SCA/official IPC LBVH paid performance and
  originality gate STILL has no demonstrated positive result.
  Detecting physical impacts is not measuring broadphase speed.

- The CENTRAL thesis is still a correctly maintained, fully paid
  SCA broadphase competing against official IPC Toolkit LBVH.
  Stronger contact validation does not itself establish a speedup,
  a new incremental hierarchy, self-contact or paper novelty.

- The independent geometry audit must NOT change physical positions,
  velocities, momentum or stress; it only reads/replays accepted
  original FEM substeps and records proof statuses.
- Charts quote original immutable CSV/JSON traces; every visual reports
  units, scalar source, scene ID, frame, seed, SHA and data limitations.
- Any source edits require relevant tests, builds, diff check and exact
  account of what was and was not verified.
- No slides/PDF/paper work unless specifically requested later.

## Workstream separation and skeptical review

Geometry/correctness reviewer: assumptions, exact oracle, counterexamples,
independent event correctness and invalidation checks.

Algorithm/benchmark engineer: actual SCA updates, pinned fixed baselines,
paid costs, thermal order and matched comparison.

Solver/scene engineer: real self-contact, articulated deformables,
multiple shapes, physics quality and external data.

Visualization engineer: measured contours, embedded topology graphs,
truthful particle/pathline overlays, SDL3/Metal + VTK + Blender exports.

Reproducibility/novelty reviewer: independent code/sample audit, held-out
cohort, uncertainty and prior-art matrix. Must challenge every positive
claim and expose failed cases.

A workstream may call its own task complete only when another workstream
can reproduce the receipt. No prose may claim subagents ran when no
separate recorded execution exists. Incomplete tasks are marked pending.

## Required response at each development milestone

State changed files; source commit; exact tests and platform; raw evidence
paths; unsupported claims; regressions; next acceptance gate. Do not
report planned work as achieved.
