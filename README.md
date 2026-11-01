# Request-aware collision broad phase

A native C++23 research prototype for conservative candidate generation in deformable contact simulation. The main experiment runs **one to three interacting cloth sheets and two moving spheres** with XPBD-style constraints, discrete self-contact, and an online region controller. The physics, broad phase, precise contact response and GLFW/OpenGL viewer run in one process.

```sh
./scripts/prepare_meeting.sh   # Release build, tests, stability and benchmarks
./open_multicloth_demo.sh     # primary interactive research demo
```

The macOS viewer requires the existing GLFW and Bullet installations. Headless core/tests build on Linux; Bullet is required for the external DBVT comparison (`-DCCP_REQUIRE_BULLET=ON`). No new dependencies are introduced by the audit.

Current presentation: [20-slide research deck](output/Online_Common_Broad_Phase_Research_Layout_Final.pptx). See the [measured results](output/review-2026-11-02/RESULTS.md), [verification record](output/review-2026-11-02/VERIFICATION.md), and [layout validation receipt](.presentation-refine-2026-11-02/validation-final.json).

## Research question

Can one geometric bounds-and-request interface reduce the **total collision workload**, with runtime changes in region size and search strategy, while preserving the contact pairs required by its caller?

The implemented online controller observes local AABB work and actual contact corrections. It can split hot mesh patches, merge quiet sibling patches, and switch flat versus hierarchical region traversal. These are broad-phase association changes: they do not remesh the cloth or change its constraints. Every cached block is certified and rechecked against current padded bounds before its pairs reach the precise stage.

This is an experimental policy built from established broad-phase ideas. Publication-level novelty and general superiority have not been established. The official IPC, OGC, CCFR and PositionBasedDynamics implementations are not integrated.

## Native controls

| Control | Action |
| --- | --- |
| Space / N / R | Pause or resume / one physics step / reset |
| Left drag fabric | Pick a triangle and pull its barycentric attachment |
| Right drag or Shift + left drag | Orbit camera |
| F / scroll | Frame the full scene / zoom |
| Q / T | Cycle broad-phase policies / toggle online controller |
| G | Show live measurements and controller event log |
| S | Toggle self-contact; invalidates affected candidate state |
| L / O | Change sheet / obstacle count |
| P / J / Z / Y | Explicit fixed-policy ablations; see native runbook |
| K | Switch to historical single-cloth experiment |
| 1–4 | Inspect geometry, bounds, candidates and precise proximity |
| Escape | Exit |

The visual cutaway is optional and disabled initially. Camera framing and display clipping are separate from collision candidate correctness. See [native cases](docs/NATIVE_CASES.md) for reproducible capture commands and the manual drag walkthrough.

## Build and verify

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCCP_REQUIRE_BULLET=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/ccp_cloth_stability_probe 480
```

The meeting script also produces seven-policy coupled comparisons for 1/2/3 cloths, an online-versus-SAP time series, patch-size ablations, and a nine-method isolated AABB comparison including actual Bullet DBVT. **Isolated query time and complete simulation-step time answer different questions.** Do not rank them in the same chart. Coupled multi-cloth timings include periodic exhaustive coverage audits; the run metadata must identify that overhead. The historical single-cloth figures in `output/native/benchmark-summary.txt` are not current results.

## Code and evidence

- [Core audit and invocation map](docs/CODE_AUDIT_CORE.md): safety fixes, algorithm call paths and regression checks.
- [Evidence methods audit](docs/EVIDENCE_METHODS_AUDIT.md): benchmark boundaries, CSV validation and reproducibility.
- [Online algorithm](docs/ONLINE_ALGORITHM.md): feedback signals, split/merge rules, traversal selection and certificate argument.
- [Multi-cloth contract](docs/MULTICLOTH_CONTRACT.md): shared geometry and solver boundary.
- [Self-contact and mouse dragging](docs/SELF_COLLISION_AND_DRAGGING.md): precise features, exclusions and attachment constraints.
- [Stability diagnosis](docs/CLOTH_STABILITY_FIX.md): reproduced instability, corrective measures and limits.

The primary implementation is `src/research_multicloth.cpp` plus `src/online_partitioner.cpp`. The viewer is `native/main.cpp`; `include/ccp/cloth_interaction.hpp` provides picking geometry. `src/live_cloth.cpp` retains the solver-independent global candidate service and the original single-cloth fixture. The older rigid-sphere/island core, HTML illustration and Blender replay are supporting historical experiments; they do not prove current multi-cloth behavior.

The contact solver remains approximate and discrete. It can miss fast continuous crossings; self-contact corrections are not an intersection-free guarantee. Stability checks and oracle coverage are necessary safeguards, not a substitute for physical calibration or a CCD proof. Performance results are local measurements of specified fixtures; consult the dated evidence bundle and presentation for the current results and limitations.
