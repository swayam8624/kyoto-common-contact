# Phase 15 — cache-local persistent spatial cells: exactness, 1M results, and remaining negative evidence

## Why this experiment was necessary

Phase 13's one-million-object Mac slab test showed full grid rebuilding cost of 224.787ms per frame with just 14.461ms in the actual query. An initial persistent grid (Phase 14) repaired only changed cell memberships, but kept the cell-owned ID lists inside `unordered_map<Cell,Bin>` allocator nodes. Linux CI proved it was **slower**, despite skipping rebuilds: slabs 375.677ms persistent vs 327.456ms fixed and burst 713.542ms vs 492.918ms. This is a useful measured negative control, not evidence of successful optimization.

Phase 15 retains Phase 14 unchanged and creates a new `FlatPersistentGrid` and native `ccp_cbp_pressure_incremental_flat_cpu` with a contiguous `vector<Bin>` for pair enumeration. The `unordered_map<Cell,index>` remains ONLY an update lookup. Reverse per-object slot references make cell-range repair exact. A bin can be moved with swap-pop when emptied; its map index is fixed, while object reverse memberships refer to stable Cell keys, not vector slots. No empty historic cells leak across temporal motion. Every frame still pays an O(N) current-AABB range validation. A >30%-dirty frame forces a charged full rebuild.

Both variants execute at most one exact method per frame: canonically owned current-AABB 3D grid or a bound-checked dense X-sweep. There is NO future-frame oracle or workload-name-based decision. Output is count + independent 64/32-bit commutative pair fingerprints, not a full candidate pair buffer or a deformable solver.

## Passed exact tests, including failure recovery

- C++23 with -Wall -Wextra -Wpedantic -Werror compiled on Linux.
- Direct 160-frame exhaustive brute force oracle: identical input, within-cell physical movement (no stale overlapping pair predicates), cell crossing and reverse-slot repair, dense all-overlap, object count migration, corrupted input recovery.
- Independent deterministic 8-cohort cross-backend comparison against frozen global grid and X-sweep, with full count/64/32-bit pair hash equality for all completed rows. Sparse, burst, slabs, teleport, bridge, all-overlap, plus 8K dense and 65K quiet-movement workloads.
- Held-out million-AABB slabs (6 frames), million-AABB burst (20 frames) and 8,192-object all-overlap (6 frames) exact receipts. In 1M slabs, first-frame full build and five subsequent zero-cell-range-change frames are recorded separately. A burst triggers three full rebuilds total; no uncharged persistent state is assumed.

Successful Phase 15 CI experiment and downloadable per-frame CSVs: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44328310781

## Measured Linux CI, held-out seed 95377

| Workload | Flat persistent full mean | Frozen fixed grid full mean | Pointer-based persistent full mean (prior CI, context only) | Full exact result |
|---|---:|---:|---:|---|
| 1,000,000 slabs, 6 frames | **164.1018 ms** | 345.4347 ms | 375.6775 ms | 2.105x vs fixed grid |
| 1,000,000 burst, 20 frames | 472.7622 ms | **412.0671 ms** | 713.5418 ms | **negative**: flat still 14.7% slower vs fixed grid |
| 8,192 all overlap, 6 frames | **157.6340 ms** | 1306.8383 ms | (Phase 14: 178.9677 ms) | 8.290x vs grid, but fixed X sweep was faster at 147.5889 ms |

These numbers are an exploratory, separate-process Linux CI run. The pointer-vs-flat values in the preceding table include Phase 14's DIFFERENT run and should not be interpreted as strictly paired. In the Phase 15 SAME RUN, excluding the first frame, flat vs pointer means were:

| Workload | Flat after first frame | Pointer-based after first frame | Ratio |
|---|---:|---:|---:|
| 1M slabs, **full** | 95.9827 ms | 326.2566 ms | 3.399x |
| 1M slabs, enumeration only | 78.9224 ms | 309.1939 ms | 3.918x |
| 1M burst, **full** | 482.4160 ms | 619.2425 ms | 1.284x |
| 1M burst, enumeration only | 442.7611 ms | 578.8049 ms | 1.307x |

### Why slabs are STILL not 60-FPS full simulation

Flat implementation averages 164.102ms including first-frame construction; post-first mean is still 95.983ms, far above a 16.67ms 60 FPS budget. Its enumeration alone takes 78.852ms mean (and 78.922ms post-first). This is still just broad-phase candidate count/fingerprints, not full pair-list emission, narrow phase, solver, deformation or rendering. The 2.105x improvement is relative to this CPU baseline, not equivalent to actual GPU kernel timing.

### Why burst still needs a different algorithm

1M burst generated 31,500,000 exact candidate pairs on its dense frames. Most persistent elapsed time is enumeration (~425.351ms across all 20 frames) rather than maintenance (~36.187ms mean). Under such interactions, the flat persistent method is ~15% worse than a newly built fixed grid in the same independent Linux CI. Persistence cannot asymptotically eliminate writing a true K-pair list when K is large. Denser scenes need better local ownership, avoiding duplicate work, faster pair consumers, more compact occupancy layout and a principled decision to rebuild or retain.

## Reproduce on your Mac — all historical evidence remains immutable

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/common-broadphase-orchestrator
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu \
  --target ccp_cbp_pressure_cpu ccp_cbp_pressure_incremental_flat_cpu \
           ccp_cbp_pressure_incremental_flat_tests --parallel 4
ctest --test-dir build/kyoto-final-cpu \
  -R '^cbp_flat_persistent_cell_(direct|independent)_oracle$' --output-on-failure
mkdir -p output/cbp-phase15-mac-01

run_flat_case() {
  local SCENE="$1" N="$2" FRAMES="$3"
  local FIXED="output/cbp-pressure-serious-mac-01/${SCENE}-n${N}-f${FRAMES}-cpu.csv"
  local NEW="output/cbp-phase15-mac-01/${SCENE}-n${N}-f${FRAMES}-flat.csv"
  local REPORT="output/cbp-phase15-mac-01/${SCENE}-n${N}-f${FRAMES}-RESULTS.md"
  if [[ ! -f "$FIXED" ]]; then echo "Frozen CPU evidence missing: $FIXED"; return 1; fi
  if [[ ! -e "$NEW" ]]; then
    ./build/kyoto-final-cpu/ccp_cbp_pressure_incremental_flat_cpu \
      "$NEW" "$SCENE" "$N" "$FRAMES" 61749 \
      300000000 25000 12000000 || return 1
  fi
  if [[ ! -e "$REPORT" ]]; then
    python3 scripts/analyze_cbp_pressure_incremental_flat.py \
      "$NEW" "$FIXED" --report "$REPORT" || return 1
  fi
}

run_flat_case slabs 1000000 6
run_flat_case burst 1000000 20
run_flat_case all_overlap 8192 6
open output/cbp-phase15-mac-01
```

## Scientific next step

A physically meaningful contribution must avoid rehashing/pair comparison overhead on the dense 31.5M-candidate frames while retaining exact output rights. Maintain a certifiable, topology-aware interaction frontier with local dirty contact partitions, choose persistent or rebuild per region, support CPU/GPU-native ownership maintenance rather than CPU sorting for GPU, verify emitted pair bytes (not only commutative digests), and measure coupled cloth/self-collision/CCD quality against optimized external baselines. The present code demonstrates an important cache-local data structure optimization, **not** a mathematical new broad-phase algorithm, and cannot be claimed SIGGRAPH-ready.
