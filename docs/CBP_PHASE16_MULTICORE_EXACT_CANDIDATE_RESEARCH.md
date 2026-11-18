# Phase 16 — independent exact multi-core canonical-cell enumeration

**Status:** implemented on existing Phase 15 flat-persistent grid without copying either the original grid or the CLI. Existing `ccp_cbp_pressure_incremental_flat_cpu` defaults to the original single-threaded behavior. Optional `--threads 1..32` chooses the new bounded parallel enumeration when there is sufficient exact cell work. Frozen global-grid, X-sweep, Phase 14 pointer-based negative control, Phase 15 serial implementation and Mac evidence are unchanged.

## Motivating measured Mac evidence (user machine, 18 Nov 2026)

| Synthetic current-AABB workload | Phase 15 Mac full mean | Frozen global grid Mac full mean | Actual issue |
|---|---:|---:|---|
| 1,000,000 slabs ×6 frames | **74.2079ms** | 233.3483ms | Cold build 315.246ms; subsequent five frames average **26.000ms** despite ZERO dirty cell memberships |
| 1,000,000 burst ×20 frames | **383.4863ms** | 392.4353ms | 20 frames contain **1,195,156,196** total cell-level pair tests, **315,776,871** true candidate pairs; 73.6% of visits do not yield an accepted pair |
| 8,192 all-overlap ×6 frames | **130.5792ms** | 1266.3298ms | All 33,550,336 unordered pairs overlap; fixed serial X-sweep is better still at 127.2662ms |

**Interpretation:** flat contiguous cell ownership and temporal reuse are no longer the only issue. Bursts pay heavily for independent pair tests, canonical-cell ownership checks and digest mixing. The 1M slabs still take ~15.15ms query-only per warm frame plus ~10.85ms in validation/maintenance. Correctness here means exact current-AABB candidate set as verified by count + independent 64/32-bit pair fingerprints, NOT full pair materialization, CCD or physically resolved collisions.

## Implementation: bounded disjoint cell work

`FlatPersistentGrid::enumerateParallel(maxTests,maxMs,threads,&actuallyUsed)` reads the SAME persistent current-coordinate boxes and contiguous cell membership lists used by Phase 15. Only the exact candidate enumeration step runs in parallel; all cell occupancy maintenance is completed and charged before workers start.

1. Preflight ensures the complete cell-local comparison count `workUpperBound` fits the user-specified comparison budget. If not, the established serial budget-capped algorithm runs and reports incomplete rather than pretending a partial digest is a full result.
2. The spatial cells are assigned to disjoint contiguous chunks, balanced by each cell's exact `binSize*(binSize-1)/2` pair count plus a minimal scan cost. This does not assume any specific scene's group labels or separability.
3. Worker threads read immutable current AABB boxes and cell lists, compute local exact pair digests and counts and apply unchanged canonical overlap-minimum cell ownership. No digest is shared mutably, preventing data races.
4. On completion, worker-local digests combine using commutative modular sum and XOR. All results are bitwise identical to serial and to the separately executed independent frozen global reference on completed frames. Incomplete/timeout frames remain incomplete. Worker creation, chunking, joining and reduction are inside the measured query.
5. The same command with `--threads 1` is the serial comparator; `--threads 2`, `4` or `8` records requested and **actual workers** in new CSV columns. Legacy positional commands retain single-threaded operation.

## Correctness gates

- 160-frame original brute force regression maintained in `cbp_flat_persistent_cell_direct_oracle`.
- New dense 512-body all-overlap direct brute oracle, with 2, 4 and 8 genuine workers and repeated identical temporal frames. Its 1,046,528 visited cell pairs and 130,816 unique real pair candidates are all accounted for.
- Independent 65,536-object slabs, 16,384-object burst and 1,024-object all-overlap tests compare serial/4-thread digests against full independent fixed-grid or sweep references.
- Phase 16 Linux CI exercises 1M slabs ×6 and 1M burst ×20 with serial vs 4 threads vs independently run frozen global grid. ALL tests passed: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44333431570

## First measured Linux CI (seed 95377) — NOT Mac measurements

All numbers below are **warm-frame means excluding frame 0**, and are from independently executed serial and parallel processes on an Ubuntu shared runner. The frozen-grid control's mean (where shown) includes all frames and is therefore not a directly paired warm-only comparator.

| Workload | 1-thread flat persistent FULL | 4-thread flat persistent FULL | Warm-frame full speedup | Warm enumeration speedup |
|---|---:|---:|---:|---:|
| 1M slabs, ×6 frames | 41.2478ms | **31.6178ms** | **1.305×** | 1.659× |
| 1M burst, ×20 frames | 473.1819ms | **233.1719ms** | **2.029×** | 2.257× |

All tested million-object frames used **four actual workers**, not emulated parallelism. For the 20-frame burst, both algorithms returned exactly **315,776,663 true candidate pair occurrences** from **1,195,007,152** cell-local comparisons, with every frame's complete 64/32-bit digest equal to the independent global reference. **879,230,489 (73.6%)** of visited cell-pair checks did not produce a unique accepted candidate (false spatial-cohabitation pair tests and duplicate-cell filtering).

The speedup is useful engineering but not universally portable. A 4-worker CPU query on shared Linux CI may behave differently on an Apple Silicon machine with performance/efficiency core topology, different memory bandwidth, and power limits. Additional cores will not automatically make a one-million-body solver interactive, and all-overlap explicit output still has Ω(K) complexity.

## Reproduce on the original Apple Silicon machine

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/common-broadphase-orchestrator
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu \
  --target ccp_cbp_pressure_cpu ccp_cbp_pressure_incremental_flat_cpu \
           ccp_cbp_pressure_incremental_flat_tests --parallel 4
ctest --test-dir build/kyoto-final-cpu \
  -R '^cbp_(flat_persistent_cell_direct_oracle|flat_persistent_parallel_independent_oracle)$' \
  --output-on-failure

mkdir -p output/cbp-phase16-mac-01

run_multicore_case() {
  local SCENE="$1" N="$2" FRAMES="$3"
  local FIXED="output/cbp-pressure-serious-mac-01/${SCENE}-n${N}-f${FRAMES}-cpu.csv"
  if [[ ! -f "$FIXED" ]]; then echo "Missing independent frozen CPU evidence: $FIXED"; return 1; fi
  for T in 1 2 4 8; do
    local OUT="output/cbp-phase16-mac-01/${SCENE}-n${N}-f${FRAMES}-threads${T}.csv"
    if [[ ! -e "$OUT" ]]; then
      ./build/kyoto-final-cpu/ccp_cbp_pressure_incremental_flat_cpu \
        "$OUT" "$SCENE" "$N" "$FRAMES" 61749 \
        300000000 25000 12000000 --threads "$T" || return 1
    fi
  done
  for T in 2 4 8; do
    local REPORT="output/cbp-phase16-mac-01/${SCENE}-n${N}-f${FRAMES}-threads${T}-REPORT.md"
    if [[ ! -e "$REPORT" ]]; then
      python3 scripts/analyze_cbp_pressure_parallel.py \
        "output/cbp-phase16-mac-01/${SCENE}-n${N}-f${FRAMES}-threads1.csv" \
        "output/cbp-phase16-mac-01/${SCENE}-n${N}-f${FRAMES}-threads${T}.csv" \
        "$FIXED" --report "$REPORT" || return 1
    fi
  done
}

run_multicore_case slabs 1000000 6
run_multicore_case burst 1000000 20
open output/cbp-phase16-mac-01
```

Outputs are immutable and auto-preserved when the command is rerun. Run orders should be randomized and repeated for stronger statistics, rather than selecting whichever thread count won after inspecting one sequence and reporting it as a held-out result. No GPU kernels are run by this new CPU-only experiment.

## Boundary of originality — research prior art

**Parallel spatial subdivision and sort-and-sweep are established technologies, not new:** Scott Le Grand, GPU Gems 3, Chapter 32, NVIDIA, https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-32-broad-phase-collision-detection-cuda .

**Topology-/activity-aware GPU deformable collision culling is also already being studied:** Elias Füricht, *GPU-Based Collision Detection and Resolution for Elastic Polyhedral Meshes*, TU Wien, September 2026, https://www.cg.tuwien.ac.at/research/publications/2026/Fuericht-2026-GPUCDR/ ; Sung et al., *Efficient Self-Collision Culling for Real-Time Cloth Simulation Using Discrete Curvature Analysis*, Mathematics 14(9), 2026, https://doi.org/10.3390/math14091504 .

### A more ambitious, falsifiable next hypothesis

Treat the exact collision set as an evolving **interaction graph** `E(t)` rather than starting from boxes each frame. Look for an **exact, nonoverlapping edge cover by certified interaction blocks** plus a sparse residual, with topology-guided repairs and a provable fallback. A certified complete-cross block can represent many AABB pairs compactly, but a full triangle-level contact solver may still need to expand individual constraints. A genuine speedup requires the downstream consumer to exploit the structured representation without silently ignoring necessary contacts.

**To claim a conceptual breakthrough:** prove exactness of block certificates and disjoint ownership, derive update/maintenance/output complexity, compare strong external BVH/grid/CCD baselines on independent real deforming meshes, measure actual pair bytes and solver contact accuracy, and demonstrate benefits that survive no-contact, sparse, dense, tearing, and rapid-motion adversaries. None of those research requirements is established by Phase 16 alone.
