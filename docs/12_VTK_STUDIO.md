# Pass 3 — VTK Studio rendering & honest scientific appearance

Status: VTK studio renderer source now in
scripts/render_feature_vtk_studio.py. Earlier Matplotlib, Plotly and
ParaView figure pipelines remain available. The studio rendering
is a separate optional *presentation layer* over exactly the
same native C++ exported mesh geometry.

## Inputs and scientifically correct labels

The C++ exporter apps/feature_scene_export.cpp emits 61 frames of
vertex xyz and displacement, stable faces/edges, and true computed
AABB candidate IDs. The VTK studio reads those CSVs; it cannot add
new physics or invent actual contact events.

A toroidal shell passes a triangulated spherical cage in a
scripted 3D cubic trajectory (no mass, stress, elasticity,
force integration, physical contact response or general CCD).

The scalar on the shell is **vertex displacement magnitude in
world units relative to the initial frame**. It is not stress
or strain. The golden cage is a separate object. Highlighted
links are conservative vertex-face/edge-edge AABB candidates,
not impacts or penetration records.

VTK normals, transparency, multiple lights and a studio ground
grid are solely visualization choices. The grid is decorative
and is not an interacting geometry object.

## Native reproduction

Run after a successful C++ build of kyoto_feature_scene:

    ./build/kyoto_feature_scene results/feature-scene

In a Python environment with an EGL/OSMesa/Cocoa-capable offscreen
VTK installation:

    python3 -m pip install numpy matplotlib pillow vtk

    python3 scripts/render_feature_vtk_studio.py \
      --input results/feature-scene \
      --output results/vtk-studio

Outputs: studio_frame_00.png, studio_frame_30.png,
studio_frame_60.png. The studio renderer does not pretend
to generate a high-fidelity real physics simulation.

A separate GitHub Actions workflow (.github/workflows/vtk-studio.yml)
compiles the native scene, installs pinned VTK and verifies
three 1500x960 studio frames, with an artifact archive. On Mac,
the local graphical backend may require a functioning windowing
environment or a build of VTK with offscreen support.

## Visual production review

The earlier Matplotlib scientific atlas has good raw provenance,
but its four-panel physical scene can appear too small. The VTK
studio produces a larger photogenic single-frame hero. Both
remain part of a multi-stage system:

- Matplotlib: numerical graphs/measurement uncertainty
- ParaView/VTK: actual mesh surface/field and cutaway visualizations
- VTK studio: high-quality native mesh composition
- Plotly: rotatable offline mesh playback and time scrub
- Blender/Metal: future actual solver-backed cinematic export

## Remaining fidelity requirements

True surface strain or stress fields, contact impulses, physical
solver dynamics, simulated granular systems, accurate CCD event
markers, and physically calibrated real-world scenes must wait
until corresponding solver traces exist. The next scientific
visual should derive from a true solver, not decorate a kinematic
case and call it physics.
