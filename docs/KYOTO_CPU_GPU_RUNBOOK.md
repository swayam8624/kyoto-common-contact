# Kyoto CPU, parallel CPU and Metal benchmarking runbook

This branch is **implementation in progress**, not a completed large-scale empirical evaluation. There are NO newly measured CPU/GPU speedup numbers in this document. Do not copy November 2 results into new graphs as if they came from these executables.

## What is implemented on this branch

| Executable | Actual algorithm | Hardware and threading | What it measures |
| --- | --- | --- | --- |
| `ccp_scaling_cpu` | Exact 3D SAP with serial, fixed-worker, work-aware and small-scene automatic paths | 1 CPU thread / specified C++ worker count | Includes sorting, thread creation/join, collision candidate materialization and deterministic merge |
| `ccp_scaling_regional` | Project's real `RegionalCandidates`: certified region cache, fixed/adaptive padding, optional hierarchical top level | CPU, currently serial | End-to-end actual regional query including region building/cache work |
| `ccp_scaling_metal` with `sap` | GPU Metal compute SAP after CPU x-sort, with prefix-summed full pair emission | Apple GPU + host CPU sort | Host sorting/upload/encode/submit/wait, CPU prefix, GPU pair emission, and output readback; Metal kernel durations separately |
| `ccp_scaling_metal` with `exhaustive` | GPU exhaustive 3D AABB reference, with full pair emission | Apple GPU | Reference throughput/correctness including pair output, **not a competitive scalable broad-phase strategy** |

Metal now performs **two GPU passes**: exact per-object count and collision pair compaction with CPU exclusive prefix offsets. Its host timing includes host sort (SAP), input copying, queue execution, readback and validation of duplicate pair IDs; its separately reported GPU time adds the durations of both compute kernels. The CPU benchmark also materializes exact candidate pairs. Equal snapshot numeric semantics and consistent timing methodology still require final cross-device audits, because the CPU benchmark uses float geometry while the regional core uses double.

The real **online contact-work-aware partitioner** is not yet part of the multi-core/GPU scaling executables. `ccp_scaling_regional` exercises the underlying existing certified regional implementation, not the closed-loop online optimizer. The coupled cloth viewer still has a fixed 12×12-cell, at most three-sheet simulation. Scaling object AABBs alone is not scaling physical cloth.

## Build on macOS

```bash
brew install cmake glfw bullet
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCCP_REQUIRE_BULLET=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Linux the core/CPU executables do not require GLFW or Metal. `ccp_scaling_metal` is Apple-only.

## Small correct-first trials

```bash
./build/ccp_scaling_cpu --self-test
mkdir -p output/scaling/initial
./build/ccp_scaling_cpu output/scaling/initial/cpu.csv 512 20 8 hotspot 1234
./build/ccp_scaling_regional output/scaling/initial/region-fixed.csv 512 20 hotspot 1234 0.6 fixed
./build/ccp_scaling_regional output/scaling/initial/region-adaptive.csv 512 20 hotspot 1234 0.6 adaptive
./build/ccp_scaling_regional output/scaling/initial/region-hier.csv 512 20 hotspot 1234 0.6 hierarchical
# Apple Metal devices only:
./build/ccp_scaling_metal output/scaling/initial/gpu-sap.csv 512 20 hotspot 1234 sap
./build/ccp_scaling_metal output/scaling/initial/gpu-reference.csv 512 20 hotspot 1234 exhaustive
```

All generators use independent reproducible snapshot construction with the same `scene`, `N`, `frame`, and `seed`. The regional benchmark uses double-precision Bounds for the existing project core while CPU/GPU standalone benchmarks use float positions. This means cross-executable numeric semantics are **not yet identical**: treat these as exploratory profiles; a single unified serialized snapshot representation is required before making numerical head-to-head claims.

## Cautious exploratory scaling

Use `sparse`, `cluster`, `hotspot`, `dense` in increasing sizes. Start at 256, 1k, 4k, 16k and progress only after memory/correctness checks. The `all_overlap` scenario is deliberately limited to 2048 objects in CPU/regional benchmark because exact candidate output is quadratic. Metal produces full pair output and enforces a 16-million-pair safety cap.

For each run, record git SHA, hardware/CPU cores, RAM, GPU model, macOS/Linux version, compiler and flags, power/thermal state, build mode, seed, frame count, thread count and output CSV checksum. Compare **same experiment/same algorithm output semantics only**. Run at least five independent process launches, randomize order, report median/dispersion, distinguish correctness-audited frames from unaudited ones.

## Correctness evidence

- CPU serial and multithread SAP compare exact full pair vectors against an exhaustive AABB oracle in `--self-test`.
- CPU and regional scaling executions audit every fifth frame when `N<=2048`. Larger benchmark frames are **not exhaustively validated** by these programs, and should not be presented as so.
- Metal audits every fifth frame at `N<=2048` against independent CPU brute-force **full pair sets**. Larger frames are not exhaustively audited and must be clearly labeled.
- Current Metal code is a reference workload. It does **not** simulate a GPU port of the full adaptive region algorithm.

## Engineering next milestones

1. Obtain a clean CI compile/test receipt on both host systems; run real GPU measurements on user hardware. No successful build or execution is asserted until observed.
2. Introduce a versioned binary snapshot format consumed by every backend with identical float/double, padding, IDs and masks.
3. Implement a strong incremental SAP baseline, not merely SAP rebuild.
4. Generalize mesh dimensions and region ID namespace beyond 12×12 and three cloth sheets; prove partition coverage and stable/collision-free IDs.
5. Parallelize region-pair work using independent thread-local buffers and deterministic consolidation, while keeping mutable cached certificates synchronized at frame boundaries.
6. Port **the certified regional candidate algorithm**, not merely SAP, to Metal using bounds, patch membership and prefix-summed pair compaction. Time transfers, allocation, execution and complete materialization, and compare exact candidate pairs.
7. Integrate true online controller feedback for both CPU multicore and GPU using the SAME state sequence; stress transient and steady contact regimes.
8. Profile, train policy thresholds on separate scenes, preserve held-out validation and generate a presentation only from measured evidence.

**No current benchmark establishes a cross-device scaling win or a finished optimal algorithm.** The purpose of this branch is to construct that evidence honestly.
