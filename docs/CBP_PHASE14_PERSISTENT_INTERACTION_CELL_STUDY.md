# Phase 14 — exact persistent interaction-cell occupancy under real million-AABB pressure

## Research question

Phase 13's Mac 1,000,000-object slab scene spent 224.787ms/frame rebuilding a spatial hash-grid and only 14.461ms querying it. Can we preserve exact *current AABB* cell memberships across frames, only repair cell-range changes, and thereby remove repeated full rebuild work without hiding correctness costs? What happens when a dense burst forces large fractions of the geometry to change spatial cells?

This is an incremental spatial-index BASELINE based on conventional ideas. It is not the novel contact frontier, CCD, triangle-level collision or coupled deformable physics. Its exactness guarantee is for the selected global current-AABB search/count+commutative fingerprints, verified separately on frozen references and by exhaustive small brute-force tests.

## How the actual implementation works

`include/ccp/cbp_pressure_incremental.hpp` owns one `unordered_map<Cell,Bin>` and exact reverse cell membership slots per stable object ID. The algorithm:

1. Validates every source Float32 box, computes its current integer cell-coordinate range, and ensures a 256-cells-per-box / global 12-million-entry budget before modifying spatial state. This validation is O(N) EVERY frame and is charged.
2. Compares each range to the previous frame. If unchanged, it does not rebuild that object's cell memberships; it still updates the stored current AABB coordinates because overlaps can change INSIDE the same cell.
3. For changed ranges, removes old memberships with swap-pop, fixes reverse references of any swapped ID, and inserts new memberships into exact current cells. Empty spatial cells are deleted. The grid's pair-work upper bound is maintained with O(1) pair-count deltas for each addition/removal.
4. If N changes or more than 30% of objects change occupied cell ranges, it discards previous spatial memberships and fully rebuilds, charging the entire cost. This bounds the pathologically expensive per-object repair alternative.
5. Only on sufficiently dense grids, it estimates projected X-sweep work using actual current X extents. If bounded and substantially lower than the grid-work estimate, it executes ONE X-sweep instead; otherwise one current persistent-grid query. Both methods use the same stable-ID pair fingerprints and exact canonical overlap-minimum ownership.
6. Emits full separate timings for preflight, dirty maintenance/full rebuild, density decision, chosen query and whole query. No future frames, offline fastest policy or unexecuted query timings drive this choice.

**Potential failure:** In dense clusters, the persistent index may still enumerate and hash tens of millions of candidate pairs and repeated canonical-cell comparisons. In large moving scenes, a dirty repair can be MORE expensive than rebuilding. Only held-out data can decide if this design is useful. Per-scene x-sweep fallbacks still require building/maintaining the grid first, which is suboptimal.

## Correctness and evidence gate

The existing Phase 11 independent frozen grid and X-sweep executables remain untouched. `tests/cbp_pressure_incremental_tests.cpp` runs a separate direct exhaustive brute-force oracle across 160 changing frames: identical scene reuse, within-cell moving AABBs, exact cell migration, all-overlap, teleporting motion, resized object count, nonfinite rejection and oversized AABB rejection. `scripts/test_cbp_pressure_incremental.py` compares full stable-ID pair counts and complete 64/32-bit pair digests across six procedural regimes and additional high-density scenes. The CLI does not silently overwrite historical evidence.

CI experiment: https://github.com/swayam8624/kyoto-common-contact/actions/workflows/cbp-phase14-persistent.yml

## Previous Mac baseline (before persistent implementation)

| Scene | Phase 13 full query | Frozen grid full query | Observed bottleneck |
|---|---:|---:|---|
| 8,192 all-overlap | 129.233 ms | 1266.330 ms | Every true pair output counted; X-sweep works well |
| 1M static-range slabs | 239.248 ms | 233.348 ms | 224.787 ms index rebuilding, 14.461 ms query |
| 1M dense burst | 396.011 ms | 392.435 ms | 53.401 ms build, 342.610 ms query |

These are separate full-query process means, not randomized or statistically controlled paired trial claims. The all-overlap reference X-sweep itself was 127.266 ms, slightly faster than the Phase 13 controller. The older GPU Metal 12.9ms 1M burst figure is GPU kernel-only and cannot be substituted as a comparable end-to-end time.

## Repeat exact new Phase 14 measurements on Mac without overwriting Phase 11/13

```bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/common-broadphase-orchestrator
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu \
  --target ccp_cbp_pressure_cpu ccp_cbp_pressure_incremental_cpu \
           ccp_cbp_pressure_incremental_tests --parallel 4
ctest --test-dir build/kyoto-final-cpu \
  -R '^cbp_persistent_cell_(direct|independent)_oracle$' --output-on-failure
mkdir -p output/cbp-phase14-mac-01

run_persistent_case() {
  local SCENE="$1" N="$2" FRAMES="$3"
  local OLD="output/cbp-pressure-serious-mac-01/${SCENE}-n${N}-f${FRAMES}-cpu.csv"
  local NEW="output/cbp-phase14-mac-01/${SCENE}-n${N}-f${FRAMES}-persistent.csv"
  local REPORT="output/cbp-phase14-mac-01/${SCENE}-n${N}-f${FRAMES}-RESULTS.md"
  if [[ ! -f "$OLD" ]]; then echo "Missing frozen control: $OLD"; return 1; fi
  if [[ ! -e "$NEW" ]]; then
    ./build/kyoto-final-cpu/ccp_cbp_pressure_incremental_cpu \
      "$NEW" "$SCENE" "$N" "$FRAMES" 61749 300000000 25000 12000000 || return 1
  fi
  if [[ ! -e "$REPORT" ]]; then
    python3 scripts/analyze_cbp_pressure_incremental.py \
      "$NEW" "$OLD" --report "$REPORT" || return 1
  fi
}

run_persistent_case slabs 1000000 6
run_persistent_case burst 1000000 20
run_persistent_case all_overlap 8192 6
open output/cbp-phase14-mac-01
```

Existing frozen files are read-only references. New Phase 14 outputs never overwrite earlier runs, and a capped frame is never an exact full result.

## Longer-term novelty requirements

To move beyond a standard incremental spatial hashing optimization, we need one coherent EXACT contact topology model that (i) sustains cheap updates under deforming geometry, (ii) chooses spatial query execution at finer than the entire scene, (iii) avoids duplicate pair work near cell boundaries and output explosion when possible, (iv) supports GPU-owned index maintenance rather than CPU-building a grid for GPU kernels, (v) measures actual pair output/streaming and physics resolution, and (vi) shows a rigorous improvement against strong dynamic BVH, incremental SAP, broad-phase CUDA/Metal/CPU and deformable simulation literature on independent datasets.
