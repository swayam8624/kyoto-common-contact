# Common Contact Pipeline

An independent, inspectable prototype for comparing broad-phase candidate generation while keeping sphere contact checks and response explicit. It is not an integration of IPC, OGC, PBD/XPBD, or CCFR, and it makes no performance claim for those methods.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./run_demo.sh
```

Open `http://127.0.0.1:4173`. `run_demo.sh` builds the C++ service and launches the Node bridge, which starts one persistent C++ process. The C++ service owns all simulation and collision state; the browser only renders returned snapshots and sends reset, step, and broad-phase commands. Close the server with Ctrl+C.

## Implemented stages

- Motion: deterministic fixed step (1/120 s), bounded 3D sphere state.
- Broad phase: brute-force reference and deterministic sweep-and-prune; both support endpoint AABBs and conservative linear swept AABBs.
- Narrow phase: sphere-sphere distance plus analytic sphere-sphere time of impact in swept mode.
- Response: unit-restitution normal impulse with inverse-mass position correction; this is a demonstration-specific response.
- Graph: connected components over verified contact edges; isolated bodies appear as singleton components.
- Viewer: scene, broad-phase, and query-mode switching; pause, single-step, reset, candidate/contact overlays, swept/free-flight paths, island colors, pair ledger, and measured local timings.
- Capture: record and replay deterministic command sequences against the live C++ service; load saved replay files; export the current frame snapshot as JSON or body/contact rows as CSV. These are inspection exports, not benchmark datasets.

## Scenes

`elastic` tests an isolated head-on equal-mass collision; `islands` creates two separate contact clusters; `dense` makes broad-phase candidate reduction visible; `tunneling` compares a fast sphere crossing with discrete endpoint checks and swept sphere TOI. All bodies are spheres in a bounded 3D box. Swept bounds are conservative for linear sphere translation over each fixed step; mesh rotation/deformation, mesh proximity, benchmark campaigns, and method adapters are not implemented.

## Protocol

The service accepts one JSON object per stdin line with `command` set to `status`, `step`, `reset` (and `scene`), `broadPhase` (and `mode`), or `queryMode` (and `mode`: `discrete` or `swept`). Each response echoes the request `id`, includes `ok` and a snapshot, and uses protocol version `1`. Logs go to stderr.
