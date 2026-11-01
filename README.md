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

The published-library comparison includes a stronger **persistent Bullet
DBVT update** row, alongside fresh Bullet rebuild. Importantly, the
temporal Bullet strategy updates its spatial tree, whereas guarded SAP
reuses a certified pair *superset*. Compare returned pair counts and
downstream precise work, not query time alone.


## Live multi-cloth research experiment (new)

Use `./open_multicloth_demo.sh` for a continuously excited, physically interacting **three-cloth + two-moving-sphere** research scene. It compares full SAP, the older global guard, and a new experimental locally certified region-pair method that revalidates cached candidates against current tight AABBs. Press K for the original one-cloth demonstration; L changes the cloth count; O changes spheres; Q changes the policy; G shows the live diagnostic/time-series panel. Benchmarks and candid novelty limits are in `docs/MULTICLOTH_CONTRACT.md` and generated `docs/MULTICLOTH_RESULTS.md`.


### Hierarchical region and patch-size experiments

The new optional two-tier region sweep is enabled in the native research viewer by J. It prunes region-envelope pairs before consulting local candidate caches, while preserving a flat-pair ablation. The P key cycles mesh patch sizes (1, 2, 3, 4, 6, 12 cells). Repartitioning invalidates regional caches but never changes the physical solver.

Run the normal meeting-preparation script to generate patch-sweep.csv, patch-sweep.svg, and PATCH_CALIBRATION.md. The 12-way sweep chooses a region size using one earlier time window and evaluates it on a later window. It is an offline ablation, not an online trained algorithm or claimed publication novelty.

Press Y in the native multi-cloth demo to apply the policy selected from your local earlier calibration frames in patch-sweep.csv. This configures the *actual running region candidate system* without changing the solver. It is offline profile-guided static configuration, NOT an online adaptive partitioner. The timed profile excludes the optional exhaustive oracle after preflight but checks physical positions against SAP on every frame.


## Live online region split/merge policy (new research default)

`./open_multicloth_demo.sh` now starts a **genuine online controller**: topology-aligned 6×6 cloth patches may split to 3×3 or merge with quiet siblings into 6×6/12×12 regions based on **actual local broad-phase work and confirmed contact corrections**. The controller may switch flat versus hierarchical broad-phase traversal using live occupancy and observed time feedback. It changes only the broad-phase patch association, NEVER cloth particles, constraints, or precise contact formulas. Its split/merge decisions run at frame boundaries and invalidate stale region certificates. The old SAP/fixed/global guard alternatives remain. Press T to toggle the online policy directly, Q to cycle policies, and G for live event logging. P/J/Z/Y override into fixed-control mode for explicit ablations.

A reproducible online-versus-SAP coupled simulation trace runs as part of `scripts/prepare_meeting.sh` and produces `output/native/online-controller-trace.svg`, raw `.csv`, and `docs/ONLINE_CONTROLLER_RESULTS.md`. These output measurements include controller overhead, time-series of patch count and actual inter-sheet contact corrections, and physical-state equivalence. **Not publication-level novelty or proven superiority:** online partitioning and deformation-aware BVHs have prior art; PhysX ABP/PABP and the 2026 deforming BVH work remain strong external comparators.
