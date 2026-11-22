# Kyoto: Spacetime Contact Atlas (research reset)

**Status: STAGE 2. Analytical swept-boundary and temporal-refinement studies; no full solver, independent general CCD, published LBVH comparison, SCA hierarchy, or novelty claim.**

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



## Stage 2 — synchronized interval certificates and selective pair refinement

The C++23 temporal_refinement.hpp supports conservatively bounded
cubic paths split into synchronized slabs using interval de Casteljau.
It tests both a global time-slab sweep and a selective per-candidate
refinement against independent exhaustive pair enumeration.

The local synthetic study has 120 measured method-condition rows, 96
randomized constructive continuous-contact test scenes and two
analytical false-positive families.

**Critical negative:** selective is around 3x faster than global
at 16 slabs in the local 2,048-target example, but the one-slab
baseline remains far faster. This does NOT establish acceleration
of a real collision solver or superiority to official IPC LBVH.

    ./build/kyoto_temporal_experiment results/temporal-study
    python3 -m pip install numpy matplotlib pandas plotly pillow
    python3 scripts/render_temporal_study.py \
      --input results/temporal-study \
      --output results/temporal-study/visuals

The produced scientific atlas includes six figures, measured 3D
cost surfaces, a 3D candidate network and a standalone offline 3D
observatory. Every chart is traceable to the native experiment CSV.
For theory and exact constraints read docs/09_CERTIFIED_TEMPORAL_REFINEMENT.md.

Official IPC Toolkit stable tag pinned for the NEXT adapter: v1.6.1
(19bcab7e1f5a0d2addfcd28513422ff248ea504e). Its vertex-face and
edge-edge candidate API must not be confused with this experimental
triangle-triangle pair contract.


Read the docs in numbered order:
- docs/01_PROBLEM.md
- docs/02_STRATEGY.md
- docs/03_VISUALS.md
- docs/04_RESET.md
- docs/05_PRIOR_ART.md
- docs/06_METHOD_BLUEPRINT.md
- docs/07_TASK_MATRIX.md
- docs/08_CERTIFIED_MOTION_LAB.md
- docs/09_CERTIFIED_TEMPORAL_REFINEMENT.md
- AGENTS.md

The next gate is a pinned IPC Toolkit LBVH plus independent CCD adapter.
Do not substitute candidate parity for true continuous collision proof.
