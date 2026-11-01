# Blender cloth and mesh bounds query

This folder contains a native Blender cloth experiment, five evaluated triangle-mesh snapshots, their C++ sphere-to-mesh query results, and three Blender Cycles captures.

## Provenance

`Cloth_Simulation_Source.blend` contains a Blender Cloth modifier on the 35 × 35 cloth grid, with collision modifiers on the fixed sphere and ground. The simulation runs over frames 1–90 at 30 fps. `.render3d/simulate.py` evaluates the cloth at each frame, records its evaluated positions and triangulated faces, and exports snapshots at frames 001, 020, 035, 060, and 090. Each snapshot has 1,225 vertices and 2,312 triangles.

`mesh_bounds_query` reads each snapshot and tests it against the fixed sphere at `(0, 0, 0.9)` with radius `0.8` and margin `0.05`. It creates padded `PrimitiveBounds` for the sphere and each triangle, generates sphere/triangle AABB candidates with sweep-and-prune (SAP), and checks those candidates against an exhaustive AABB oracle. It then applies the sphere-to-triangle distance test, `distance <= radius + margin`. `verifiedSameOracle` compares the SAP candidate set to the oracle; `misses` lists proximity triangles absent from the candidate set.

`.render3d/render.py` builds the frame 060 analysis scene from the exported mesh and `query-060.json`, then renders `geometry.png`, `candidates.png`, and `proximity.png` with Blender Cycles. The `Common_Broad_Phase_3D.blend` file contains the native scene and shape-key replay of the evaluated cloth positions. Its `Simulated cloth` evaluated vertices at frame 060 were checked against `frame-060.mesh`; the maximum per-vertex difference was about `1.2e-7` Blender units.

## Observed query results

All five query files reported 2,312 triangles, `verifiedSameOracle: true`, no proximity misses, and a positive minimum gap:

| Frame | AABB candidates | Within margin | Minimum gap |
|---:|---:|---:|---:|
| 001 | 0 | 0 | 0.645393 |
| 020 | 904 | 337 | 0.023364 |
| 035 | 2,146 | 1,051 | 0.022540 |
| 060 | 1,943 | 1,037 | 0.022336 |
| 090 | 1,884 | 1,027 | 0.020561 |

These results describe the five exported snapshots and the stated sphere distance query. A positive minimum gap means the cloth triangles remain outside the sphere surface in these evaluated frames. The query is a read-only analysis of exported geometry: Blender's cloth solver performed the simulation, while the C++ code did not replace or participate in Blender's collision solver. The results do not establish equivalence to IPC, OGC, PBD, or XPBD contact predicates or solvers.

## Open and play the Blender files

- Open `Cloth_Simulation_Source.blend` to inspect the cloth, collision objects, and Blender physics setup. Its cloth point cache is not baked; play the timeline to simulate it in Blender.
- Open `Common_Broad_Phase_3D.blend` for the saved shape-key replay. Select `Simulated cloth` and play or scrub frames 1–90. This replays recorded evaluated positions; it does not rerun the cloth solver.
- The final scene opens at frame 060 and includes the fixed sphere, ground, camera, and lights. The three PNGs are Cycles renders of the exported frame 060 mesh with C++ query classifications.

## Regenerate

Run these commands from the repository root. The Blender scripts currently use the checkout path `/Users/swayamsingal/Desktop/Programming/kyoto-common-contact` for their input and output locations.

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python .render3d/simulate.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --target mesh_bounds_query --parallel
for frame in 001 020 035 060 090; do
  ./build/mesh_bounds_query "output/research3d/frame-${frame}.mesh" "output/research3d/query-${frame}.json"
done
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python .render3d/render.py
```

The simulation script creates the source `.blend`, the five `.mesh` snapshots, and the replay `.blend`. The C++ command writes one JSON result per snapshot. The render script uses frame 060's mesh and query output to create the three Cycles captures and save the final replay scene. `--factory-startup` avoids loading user-installed Blender add-ons in background runs.

## Native stages and video

Run `./open_3d_demo.sh` from the project directory for a clean Blender startup. The View Layer dropdown switches **01 Physics replay**, **02 Candidates · frame 60**, and **03 Proximity · frame 60**. Space plays the recorded mesh; the two query layers show a static analysis of frame 60. Their front quadrant is omitted only to reveal the sphere. All C++ classification uses the full mesh.

`Common_Broad_Phase_3D.mp4` is a seven-second slow-motion playback with a held final frame, rendered from the recorded physical mesh. No generative artwork is used. Regenerate with `.render3d/video.py` in Blender followed by the ffmpeg command in the task log. `.render3d/organize.py` creates the native stage View Layers after rendering.

The final native replay was reopened headlessly with factory startup and matched all five exported mesh snapshots within 1.4e-7 Blender units. `native_replay_verification.json` records the comparison.

Video encoding after rendering:

```sh
ffmpeg -framerate 6 -i output/research3d/video_frames/%03d.png -vf "fps=30,tpad=stop_mode=clone:stop_duration=2" -c:v libx264 -crf 18 -pix_fmt yuv420p -movflags +faststart output/research3d/Common_Broad_Phase_3D.mp4
```
