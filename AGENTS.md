# Instructions for every agent working on Kyoto / Spacetime Contact Atlas

The user requested a CLEAN RESEARCH RESET. Read README and docs/01 through
docs/07 before modifying source. Do not restore V4/RBC/CBP or old slide
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
