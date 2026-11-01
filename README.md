# Common broad-phase research prototype

Run `./open_3d_demo.sh` to open the native GLFW/OpenGL application. The C++ cloth solver, shared candidate service, exact sphere–triangle narrow phase, collision response, and renderer execute in the same process. The application does not load recorded simulation meshes or use a web server.

The research question is whether one bounds-and-request interface can reduce collision candidate work while preserving the caller's precise contact requirements. The implemented experiment compares rebuilding on every solver iteration with guarded reuse of a candidate list. The service checks an enclosure certificate before every reuse; changes in geometry, request, filtering metadata or backend trigger a rebuild when necessary.

The default camera includes the whole initial cloth and obstacle. The optional visual cutaway is off. Press **F** to frame the current scene and **R** to restart the fall. See [DEMO_RUNBOOK.md](DEMO_RUNBOOK.md) for the short presentation walkthrough.

| Key | Action |
| --- | --- |
| Space / N | Run or pause / advance one physics step |
| Q | Rebuild every iteration / guarded candidate reuse |
| M | Query margin: 0.02, 0.05, 0.10 |
| B | Sweep and prune / exhaustive AABB reference |
| 1–4 | Geometry / certified bounds / candidates / precise proximity |
| F / R | Frame the whole current scene / reset simulation and camera |
| C | Enable or disable collision response (sphere and floor) |
| X / W | Optional visual cutaway / triangle wire overlay |
| Drag / scroll | Orbit / zoom |
| Escape | Exit |

The common interface is `BroadPhaseService` in `include/ccp/live_cloth.hpp`. It accepts only geometric bounds, stable IDs, eligibility metadata and a query request. It has no cloth or solver dependency. The cloth adapter requests current-iteration queries, then uses returned IDs to select exact contact checks and response. Detailed semantics and correctness conditions are in [native/RESEARCH_CONTRACT.md](native/RESEARCH_CONTRACT.md).

Build and verify on this machine:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/ccp_live_benchmark output/native/live_benchmark.csv
./open_3d_demo.sh
```

The native target requires the installed GLFW development library and macOS OpenGL/CoreText frameworks. It was compiled and exercised on Apple M2 Pro / OpenGL 4.1 Metal. The launcher configures/builds only if the executable is absent; rerun the build command after editing source.

**November 2 repair-branch note:** The saved baseline under `output/native/benchmark-summary.txt`
was measured with an *exhaustive precise-distance scan in every solver iteration*.
It is historical and must not be passed off as a result from the candidate-only
production narrow phase. On the repair branch run `./scripts/prepare_meeting.sh`
to rebuild, test, perform independently oracle-checked preflight, time the
production path without the exhaustive oracle, and create a fresh SVG and
Markdown report. The per-frame global **visualization** scan still runs and is
timed separately. See `docs/MEETING_READINESS.md` for the presenter workflow.

The recorded local benchmark uses five paired trials of 300 steps, excluding the first 30 steps per trial from timing summaries. Both variants use the same solver, SAP backend, geometry, timestep (1/120 s), margin (0.05), and displacement budget (0.04). Mean / maximum CPU step time was **2.351 / 4.184 ms** for rebuilding and **0.999 / 2.951 ms** for guarded reuse. All compared particle positions matched exactly in this run and the exhaustive required-pair oracle reported zero omissions. Timing includes the diagnostic oracle and excludes rendering; this is one synthetic fixture, not a general performance claim. Raw measurements and settings are in `output/native/live_benchmark.csv` and `benchmark-summary.txt`.

This is a small XPBD-style cloth/contact implementation, not an integration with the official IPC, OGC, PBD or CCFR implementations. It does not provide swept continuous collision detection or self collision, and does not establish their collision-free guarantees. The next research step is one real method adapter, validated against that method's original candidate pipeline.

Earlier Blender and HTML experiments remain under `output/research3d` and `output/` for provenance. They are historical artifacts; this launch path uses the live C++ application.

## Published-family broad-phase baseline (real Bullet DBVT)

Run `brew install bullet` on macOS or install `libbullet-dev` on Linux,
then run `./scripts/prepare_meeting.sh`. The benchmark builds and exercises
the actual Bullet Physics `btDbvtBroadphase`, exhaustive AABB, local
sweep-and-prune, and guarded SAP. It produces
`output/native/broadphase-leaderboard.svg` and `docs/BROADPHASE_RESULTS.md`.
This is *not* a universal best-algorithm leaderboard; CPU rebuild and temporal
reuse costs are compared with full candidate-set correctness checks. The
synthetic two/three-cloth bounds workloads include geometric inter-sheet
potential pairs but no real cloth self-collision solver or physical coupling.
