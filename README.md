# Kyoto: Spacetime Contact Atlas (research reset)

**Status: STAGE 0. No claimed speedup, no full solver, no continuous
collision detection implementation, and no novelty assertion.**

Research question: Can a correctness-preserving locally maintained
swept-contact hierarchy reduce total contact-detection and solver-step
time versus a strong official IPC Toolkit LBVH on moving deformable
3D geometry, especially when contact regions are localized?

This branch starts fresh. Old V4, RBC and CBP algorithms, slide decks,
repetitive demos, output assets, and competing scripts are **absent
from the working tree**, not erased from archival Git history.

Stage-0 implements only a fixed swept-AABB SAP baseline and a separate
quadratic candidate-set oracle. If the primitives do not follow straight
vertex trajectories, the endpoint AABB hull is NOT a certified bound.

Build and validate:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Read the docs in numbered order:
- docs/01_PROBLEM.md
- docs/02_STRATEGY.md
- docs/03_VISUALS.md
- docs/04_RESET.md
- docs/05_PRIOR_ART.md

The next gate is a pinned IPC Toolkit LBVH plus independent CCD adapter.
Do not substitute candidate parity for true continuous collision proof.
