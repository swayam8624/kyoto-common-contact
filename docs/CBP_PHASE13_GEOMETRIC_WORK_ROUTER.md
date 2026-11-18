# Phase 13 — geometry-aware exact global router and verified million-AABB cost decomposition

## Scientific question

Can an inexpensive, causal current-geometry work estimate route a global AABB broad phase between a 3D canonical-cell hash grid and X-sweep without offline winner knowledge or simultaneous shadow execution?

This is an explicit heuristic baseline, not a claim of algorithmic novelty. We leave Phase 11's fixed CPU/Metal kernels, the Phase 10 four-arm scene router and their historical results unchanged.

## Controller algorithm

`ccp_cbp_pressure_hybrid_cpu` builds the exact current 3D cell index and calculates `grid.workUpperBound = sum_cells binSize*(binSize-1)/2`, the actual number of cell-local pair tests the grid implementation would attempt.

If the upper bound is under `128*N`, it chooses the grid immediately, avoiding a costly X-sort on sparse scenes. Otherwise it sorts the lower-X endpoints, obtains the exact *projected* sweep pair count using upper-bound searches in O(N log N), and chooses sweep only when projected pairs are within the bounded test budget and either `projected <= gridUpper/2` or `gridUpper > budget`. Otherwise it chooses the grid. No filename/scenario decision, no hindsight oracle, and no running the unchosen candidate algorithm in the timed query.

The controller always pays grid construction even if it later selects sweep; that is an avoidable limitation of the baseline architecture. The full query total includes index build, X-projection preflight if any, decision, and one selected exact query. Input generation is measured separately. Hash-based pair count correctness is verified against the fixed complete global grid or sweep from a SEPARATE run; small tests also use brute-force; procedural analytic truth checks slabs and all-overlap.

## Actual CI measurements, held-out seed 95377

Passed source-bound CI: https://github.com/swayam8624/kyoto-common-contact/actions/runs/44323226034

| Scene | N | Causal full query ms mean | Independent fixed-grid full ms mean | Fixed X-sweep query-only mean | Real choice |
|---|---:|---:|---:|---:|---|
| All-overlap | 8,192 | **95.94644** | **963.01903** | 100.74320 | X sweep on 6/6 |
| X-projection-degenerate slabs | 1,000,000 | **250.35568** | 269.62571 | incomplete | 3D grid on 4/4 |
| Burst with changing dense contact | 1,000,000 | 339.67812 | **294.92037** | incomplete | 3D grid on 12/12 |

Means come from separate Linux GitHub runner processes and are descriptive, not statistically matched. The all-overlap router is about 10.0x faster than fixed grid but not demonstrably faster than an already optimal fixed X-sweep; the 1M burst router is worse than the independent fixed grid by about 15%. It would be unscientific to claim universal performance superiority.

### The consequential result: full-work bottlenecks differ by scene

- For 1M slabs, hybrid backend query alone averaged **23.49547 ms**, while complete query averaged **250.35568 ms**. The remaining ~226.86 ms (~90.6%) is almost entirely current-frame spatial-index construction. The separate Phase 11 Metal kernel figure ~1.60ms is even less representative of overall throughput without CPU grid building/packing/dispatch/reduction.
- For 1M burst, hybrid backend query averaged **289.66645 ms**, complete query **339.67812 ms**. About 85.3% is actual candidate search, only ~14.7% index build and router overhead.
- For 8,192 all-overlap, there are 33,550,336 true unordered AABB pairs. Grid explores eight overlapping spatial cells, producing 268,402,688 cell-level comparisons. An X-sweep visits only 33,550,336 projected pair candidates, and the router's charged sorting/preflight averages only ~0.123 ms.

These measurements are strong architectural guidance, but neither novel per-patch topology repair nor an optimized shared CPU/Metal GPU backend has been implemented yet. Count+fingerprint output is not pair-list writing, narrow-phase collision, CCD, contact solving or visual-quality validation.

## Critical native Metal measurement caveat

The Phase 11 native Metal pressure implementation **builds the 3D cell grid on the CPU and runs the complete CPU grid oracle on each frame before GPU dispatch**. It separately records `generate_ms`, `grid_build_ms`, `cpu_oracle_ms`, `gpu_prepare_ms`, `host_submit_wait_ms` and `gpu_execution_ms`. The ~12.9 ms Metal result at 1M burst is GPU kernel time only; neither that figure nor a CPU-query/GPU-kernel ratio measures whole application throughput. The 71-second simultaneous CPU+Metal experiment also includes an additional full CPU correctness oracle executing INSIDE the Metal process, so it is not a clean CPU-versus-GPU contention test yet.

The next measurement-control change should run a deferred GPU-only compute path without a synchronous in-band CPU oracle; afterward, independently compare exact per-frame count and stronger full pair receipts to the saved CPU evidence. Keep validation costs in a separate evidence column, not hidden in the GPU execution metric. The displayed GPU time presently also excludes pair-list materialization, downstream contact solvers, and some post-dispatch CPU result reduction.

## Run against your existing Mac Phase 11 evidence

Git pulls below preserve all existing data and use fresh output paths.

~~~bash
cd ~/Desktop/Programming/kyoto-common-contact
git switch research/common-broadphase-orchestrator
git pull --ff-only origin research/common-broadphase-orchestrator
cmake --preset kyoto-final-cpu -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build/kyoto-final-cpu --target ccp_cbp_pressure_cpu ccp_cbp_pressure_hybrid_cpu --parallel 4
ctest --test-dir build/kyoto-final-cpu -R '^cbp_geometric_router_exact_oracle$' --output-on-failure

mkdir -p output/cbp-phase13-mac-01
for CASE in 'all_overlap 8192 6' 'burst 1000000 20' 'slabs 1000000 6'; do
  read -r SCENE N FRAMES <<< "$CASE"
  INPUT="output/cbp-pressure-serious-mac-01/${SCENE}-n${N}-f${FRAMES}-cpu.csv"
  NEW="output/cbp-phase13-mac-01/${SCENE}-n${N}-f${FRAMES}-hybrid.csv"
  REPORT="output/cbp-phase13-mac-01/${SCENE}-n${N}-f${FRAMES}-RESULTS.md"
  if [[ ! -f "$INPUT" ]]; then
    echo "Missing existing fixed CPU control: $INPUT"
    continue
  fi
  if [[ ! -e "$NEW" ]]; then
    ./build/kyoto-final-cpu/ccp_cbp_pressure_hybrid_cpu \
      "$NEW" "$SCENE" "$N" "$FRAMES" 61749 \
      300000000 25000 12000000
  fi
  if [[ ! -e "$REPORT" ]]; then
    python3 scripts/analyze_cbp_pressure_hybrid.py \
      "$NEW" "$INPUT" --report "$REPORT"
  fi
done
~~~

Run the already built six-regime exactness CTest before comparing timing. `read -r` in zsh accepts here strings; if the loop is pasted into a shell with different parsing, run each case explicitly. Incomplete X-sweep rows remain evidence of budget termination, not completed performance.

## Next original research direction

The cheap selector is primarily a falsifiable baseline. The real opportunity is a **persistent, exactly certified interaction frontier**. Maintain per-object/region occupancy and a canonical output responsibility table over time, using a dirty set for incremental spatial updates, a safe global fallback for topology/boundary changes, and local mechanisms that adapt to contact density. A publishable contribution would demonstrate exactness, amortized update costs, pair emission or downstream consumption, GPU/CPU co-design, memory peak, and coupled cloth/self-collision quality across independent workloads against strong BVH/SAP/grid and current state-of-the-art baselines.
