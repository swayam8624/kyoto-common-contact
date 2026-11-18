# Phase 12: sustained global collision pressure, real CPU + Apple Metal

## Why the original one-million-body run was deceptively fast

The original ccp_cbp_million_scale used 8-box pre-separated islands. It was globally exact only by a pre-proven separation invariant. The 6.693 ms Mac result was not a genuinely arbitrary/global broad phase and never claimed whole deformable physics.

For N=1,000,000, the maximum possible unordered pair count is 499,999,500,000. Explicitly emitting every pair as two uint32 IDs would need approximately 4 TB per frame. Counting candidates can be fast without the cost of materializing them; do not confuse count-only with whole contact solve.

## Measured genuine global-search CI (not Mac timing)

Run https://github.com/swayam8624/kyoto-common-contact/actions/runs/44297206696 validated 1,000,000 moving synthetic AABBs and true global query. In 12-frame burst, the dense phases each generated 31,500,000 exact AABB candidate counts and took approximately 732–738 ms for general CPU 3D hash-grid search. X-axis sweep exhausted its 300,000,000-test budget and is marked INCOMPLETE, not exact. In the six-frame 1M X-aligned slabs case, the exact 3D grid returned zero candidates in ~29–31ms while X-sweep again exhausted the budget, taking several seconds per frame.

Separate pair emission in Phase 11 materialized approximately 505MB of real uint32 pair records, rather than counts only. Buffered output and writing to /dev/null are not durable SSD timing measurements.

## Phase 12 addition: truly simultaneous CPU and native Metal pressure

The new scripts/run_cbp_dual_pressure.py starts independent native global CPU grid+sweep and native Metal grid processes on the same exact deterministic geometry and seed. They run concurrently. It records child process sampled RSS/CPU percentage, immutable input and timing receipts, native per-frame CSV output, and verifies that GPU candidate count and two 32-bit fingerprints agree with the exact CPU grid on every completed frame. It enforces total wall-time, max memory and comparison-test budgets, and terminates process groups on timeout. CPU-only is an explicit CI mode, never a simulated GPU.

Successful bounded CPU-only harness CI (the Linux CI host has no Apple Metal): https://github.com/swayam8624/kyoto-common-contact/actions/runs/44300609762

These tests measure sustained contention, NOT a proven maximum GPU utilization/temperature nor an application-level solver. Use Phase 11 separate isolated runs for method comparison; concurrent runs expose resource contention instead.

## Exact Mac build and run

~~~bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/common-broadphase-orchestrator
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu --target ccp_cbp_pressure_cpu ccp_cbp_pressure_emit ccp_cbp_pressure_metal --parallel 4

# Honest isolated calibration before simultaneous stress
python3 scripts/run_cbp_pressure_ladder.py --tier calibration --gpu --output output/cbp-pressure-calibration-mac-01

# Actual challenging global geometry, 1M dense burst/slabs among eight regimes
python3 scripts/run_cbp_pressure_ladder.py --tier serious --gpu --output output/cbp-pressure-serious-mac-01 --max-tests 300000000 --max-gpu-mib 768 --stage-timeout 900 --suite-timeout 3600

# Concurrent CPU+Metal, 1M objects, 48 frames; bounded 15 minute cap
python3 scripts/run_cbp_dual_pressure.py --output output/cbp-dual-million-burst-mac-01 --scene burst --n 1000000 --frames 48 --seed 61749 --max-tests 300000000 --max-gpu-mib 768 --timeout 900

# Cross-axis pathological case, same actual 1M global AABBs
python3 scripts/run_cbp_dual_pressure.py --output output/cbp-dual-million-slabs-mac-01 --scene slabs --n 1000000 --frames 24 --seed 61749 --max-tests 300000000 --max-gpu-mib 768 --timeout 900

open output/cbp-pressure-serious-mac-01/index.html
open output/cbp-dual-million-burst-mac-01/EXPERIMENT.json
~~~

## Actual native Vulkan vs Apple Metal

Vulkan SDK, glslangValidator and MoltenVK are required on macOS. Build ccp_cbp_pressure_vulkan only if CMake exposes that target. Then run the Phase 11 calibration with --gpu --vulkan and a fresh output path. A software Vulkan driver is not a hardware GPU, and the Vulkan exhaustive kernel is not algorithmically identical to the Metal spatial grid: raw timings are not a fair API-only comparison.

## What would actually be novel

A topology-aware EXACT interaction frontier with selectively maintained dirty sets, canonical single-ownership pair emission, adaptive local spatial partitioning, defensible memory/time bounds and a coupled deformable solver. Need independent data, full candidate output accounting, solver collision quality, repeated seeds, memory and P95/P99 before a publication claim. The worst-case explicit-pair lower bound Omega(N+K) remains unavoidable for output K; no algorithm should claim to eliminate it.
