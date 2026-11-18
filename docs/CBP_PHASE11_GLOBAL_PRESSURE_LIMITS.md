# Kyoto CBP Phase 11 — adversarial pressure, real output, real API boundaries

Research status: EXPERIMENTAL. This is not a publication-ready novelty claim, a verified whole-physics acceleration, or a demonstrated GPU utilization maximum.

## The original one-million-body illusion

The older ccp_cbp_million_scale deliberately divided moving AABBs into separately indexed eight-body islands with a PROVED nonoverlap invariant, so it could enumerate only local pair candidates. The original 6.693 ms on the user's Mac was honest FOR THAT FIXTURE, but it was never a general one-million-object global broad phase.

The new pressure harness takes 1M arbitrary current AABBs as input to actual spatial search algorithms; neither is passed procedural island labels.

## The research representation

For time t, define an exact interaction graph G(t) with vertex per AABB and an edge for every compatible overlapping unordered pair. An explicitly materializing broad phase has a lower bound Omega(N+K) where K is emitted pair count, and can be Omega(N^2) even with a perfect index.

For one million overlapping AABBs K=499,999,500,000, requiring about four terabytes for one 8-byte pair-ID output per edge. A genuinely better research abstraction must represent/stream/consume sparse and dense interaction WORK honestly, not just count overlaps fast and pretend the complete list was generated.

Measure distinct costs: input generation, spatial-index build/refit, rejected pair tests, candidate enumeration, actual pair output, solver and GPU transfer/dispatch. Do not mix them.

## Six adversarial scene families

sparse: random dispersed geometry, low K; burst: moving 64-body clusters change from sparse to dense mid-trajectory; slabs: all X projections overlap but 3D pairs are absent; teleport: stable IDs with violently changing spatial positions; bridge: intercluster crossings and cross-owner candidates; all_overlap: genuine unavoidable quadratic output.

These are SYNTHETIC AABBs, not real deformable mesh/CCD simulation.

Two independent global methods:
- global_x_sweep: full sort by minimum X and enumerate all projected candidates;
- global_3d_hash_grid: construct current occupied-cell indices, compare spatial cell candidates, and assign each true overlap to the ONE cell containing its overlap-minimum coordinate. No pre-certified island partition or per-material shortcut.

The hashed-grid method has limitations: fixed unit cells, max 256 occupied cells per input bound, memory caps, pathological dense cells, and count-only storage unless the separate pair-stream program runs. They must be exposed rather than hidden.

For small N, independent global brute comparisons validate exact pair counts and 64/32-bit pair digests. For larger N, independent sweep agreement is recorded only on COMPLETE frames. Incomplete sweep work is marked budget_stop and must never be called exact. Analytic geometry-specific pair totals additionally validate slabs, all-overlap and the dense phase of burst (not a general external-scene proof).

## Genuine million-object Linux CPU experiments

Workflow: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44295651758

One million actual moving AABBs, 12-frame burst:
- Dense frames contain exactly 31,500,000 AABB candidate pairs.
- Full general 3D spatial grid finished all frames. Its dense-frame COUNT-ONLY query times were approximately 606–613 ms; the sparse-phase query approximately 74–77 ms. Input generation and grid build are reported separately.
- Global X-sweep hit 300 million tested pairs and stopped without completing. No exact sweep result was claimed.

One million moving AABBs, six-frame X-aligned slabs:
- Global 3D grid found zero exact 3D overlaps, in around 28–30 ms query.
- X-sweep again stopped at 300 million projected tests, taking 4.3–4.6 seconds per frame.

This is a genuinely different and harder algorithmic benchmark than local island enumeration, but its generative geometry still has structure, and full alternate-algorithm enumeration at one million is intentionally unaffordable.

Earlier 65,536-object global pressure baseline:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44293351468

## REAL 252 MB pair output, not merely a count

Workflow: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44296463601

A separate 4-frame, one-million-AABB burst benchmark enumerated 31.5M distinct stable-ID candidate pairs in each of two dense frames, checked total count and pair fingerprints against a separate independent count pass, and wrote a cumulative 505,232,752 bytes of 8-byte pair records over all four frames.

The /dev/null benchmark measures real pair construction and bounded output calls but not actual persistent storage. A later CI additionally wrote 505,232,752 bytes to real files and verified filesystem sizes and pair receipts. Its 57 ms/252 MB buffered-write times primarily measure OS page-cache writes; they are NOT proven durable SSD times without fsync.

## Real compute APIs

Apple Metal: native C++/Objective-C++ real Metal Shading Language compute. Two algorithms run: a canonical hashed-cell exact search and a parallel O(N^2) exhaustive search. They count and fingerprint all exact pairs, with separate GPU kernel and command host times, independent CPU grid validation and explicit workload/memory caps. The Metal code successfully compiled in CI on a macOS runner; the CI runner has no accessible Metal GPU. An actual Mac run is still necessary for GPU performance numbers. Metal COUNT-ONLY results must NOT be presented as full GPU pair-materialization throughput.

Vulkan: an independent native Vulkan-SPIR-V exhaustive kernel, supporting macOS MoltenVK portability enumeration. The real Vulkan CI run at https://github.com/swayam8624/kyoto-common-contact/actions/runs/44295265380 used llvmpipe (SOFTWARE_CPU), not a hardware GPU. All four N=512 cases passed full independent brute CPU pair count and fingerprint checks. On physical Apple hardware with Vulkan SDK/MoltenVK installed, the same code can run on the Metal-backed Vulkan GPU and disclose its actual device name. Do not compare software Vulkan throughput with Apple GPU Metal as equivalent hardware.

Native GPU time uses command buffer GPUStartTime/GPUEndTime on Metal or timestamp queries on Vulkan when supported. The entire host call, CPU preparation and allocations must also be recorded.

Relevant reference literature:
- Broadmark, CGF: https://doi.org/10.1111/cgf.13884
- Large-scale CCD/geometry benchmark: https://continuous-collision-detection.github.io/
- MoltenVK docs: https://github.com/KhronosGroup/MoltenVK
- Apple Metal command time: https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpustarttime

## Run on Apple silicon (no old data overwritten)

Use branch research/common-broadphase-orchestrator and make a fresh output directory for each suite. Build native CMake targets: ccp_cbp_pressure_cpu, ccp_cbp_pressure_emit, ccp_cbp_pressure_metal, and optionally ccp_cbp_pressure_vulkan if Vulkan SDK and glslangValidator are available.

Start with:
    python3 scripts/run_cbp_pressure_ladder.py --tier calibration --gpu --output output/cbp-pressure-calibrate-mac-01

Then sustained stress:
    python3 scripts/run_cbp_pressure_ladder.py --tier serious --gpu --output output/cbp-pressure-serious-mac-01 --max-tests 300000000 --max-gpu-mib 768 --stage-timeout 900 --suite-timeout 3600

For 1M–2M bodies on a machine with 16 GiB or more, choose --tier extreme and a fresh output path. Never ask the test to exceed its built-in work, time or memory limits merely to produce a bigger benchmark number. If actual Metal or Vulkan cannot run, report skipped, not CPU-in-disguise.

To materialize candidate pairs at one million:
    ./build/kyoto-final-cpu/ccp_cbp_pressure_emit output/cbp-pressure-pairs-mac/million-stream.csv burst 1000000 4 61749 300000000 40000 384 /dev/null

Replace /dev/null with a fresh file prefix to write physical 8-byte pair records and plan over 505 MB of free storage. Use separate paths for repeat experiments; output evidence is never overwritten.

## What remains to earn a breakthrough claim

Prove an incremental, topology-aware *interaction frontier* update scheme that pays for changed geometry, incident edges, fallback and actual pair consumption. Do not just choose a retrospective backend winner. Compare repeated independent workloads and external published baselines, test exact discrete and swept CCD candidates, perform actual solver/cloth self-contact physics, measure CPU/GPU powers and memory, and make contact quality part of the objective. A generic dense output has a quadratic information lower bound; novelty must exploit structure when structure truly exists without assuming it gratuitously.
