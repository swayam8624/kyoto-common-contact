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


## Stage 1 — complete cubic-path envelope and 3D scientific lab

The initial common swept-AABB API remains unchanged. The new
motion_envelopes.hpp module conservatively encloses cubic Bezier
moving triangle vertex paths using control-point convex hulls.
It demonstrates an analytical mid-step triangle coincidence that
unsafe endpoint-only bounding boxes would reject.

THIS IS NOT YET a solver-coupled CCD implementation or a speedup.

Build a verified 121-frame native kinematic study and science figures:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel 6
    ctest --test-dir build --output-on-failure

    ./build/kyoto_motion_export results/motion-lab
    python3 -m pip install numpy matplotlib plotly
    python3 scripts/render_motion_lab.py \
      --data results/motion-lab \
      --output results/motion-lab/visuals
    python3 scripts/export_paraview_motion.py \
      --data results/motion-lab \
      --output results/motion-lab/vtk

Outputs: 3D trajectory certificate, time-position graph, x-y-time
spacetime view, analytical regime map, interactive offline Plotly 3D
replay, 121 time-indexed ParaView meshes, C++ source CSV, manifest.
Every figure reads source geometry rather than guessed physics.

See docs/08_CERTIFIED_MOTION_LAB.md for proof and scope;
docs/03_VISUALS.md for the full gallery;
docs/07_TASK_MATRIX.md for incomplete solver/research work.


Read the docs in numbered order:
- docs/01_PROBLEM.md
- docs/02_STRATEGY.md
- docs/03_VISUALS.md
- docs/04_RESET.md
- docs/05_PRIOR_ART.md
- docs/06_METHOD_BLUEPRINT.md
- docs/07_TASK_MATRIX.md
- AGENTS.md

The next gate is a pinned IPC Toolkit LBVH plus independent CCD adapter.
Do not substitute candidate parity for true continuous collision proof.
