#!/usr/bin/env bash
# Phase 17: one reproducible entrypoint for ALL historical material policies
# plus an independently verified materialized-pair experiment. No retraining,
# no hindsight winner, no deletion or overwrite of prior evidence.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:-output/kyoto-phase17-unified-proof}"
MODEL="${2:-}"
BUILD="${KYOTO_PHASE17_BUILD:-build/kyoto-phase17}"
FRAMES="${KYOTO_PHASE17_FRAMES:-24}"
BUDGET="${KYOTO_PHASE17_BUDGET:-128}"
SEED="${KYOTO_PHASE17_SEED:-61749}"
N="${KYOTO_PHASE17_PRESSURE_N:-4096}"
PFRAMES="${KYOTO_PHASE17_PRESSURE_FRAMES:-8}"
MATERIALS="${KYOTO_PHASE17_MATERIALS:-cloth sponge cushion soft_ball spring mixed}"
REGIMES="${KYOTO_PHASE17_REGIMES:-baseline compression shear impact rapid relax}"
SCENES="${KYOTO_PHASE17_SCENES:-sparse slabs burst bridge teleport all_overlap}"
THREADS="${KYOTO_PHASE17_THREADS:-1 2 4 8}"

if [[ -e "$OUT" ]]; then
  echo "Refusing to overwrite existing Phase 17 output: $OUT" >&2
  exit 2
fi
if [[ -n "$MODEL" && ! -r "$MODEL" ]]; then
  echo "Optional model weights not found: $MODEL" >&2
  exit 2
fi
if (( FRAMES < 9 || FRAMES > 240 || BUDGET < 16 || BUDGET > 2048 ||
      N < 128 || N > 1000000 || PFRAMES < 2 || PFRAMES > 60 )); then
  echo "Bounded Phase 17 sizes violated" >&2
  exit 2
fi

cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$BUILD" --parallel 4 --target ccp_material_export ccp_material_replay ccp_cbp_pressure_incremental_flat_cpu ccp_cbp_pressure_incremental_flat_emit ccp_cbp_pressure_emit ccp_cbp_pressure_incremental_flat_tests ccp_final_broadphase_api_tests ccp_multicloth_tests
ctest --test-dir "$BUILD" --output-on-failure -R '^(final_broadphase_public_api_exact_pair_oracle|cbp_flat_persistent_cell_direct_oracle|learned_cpp_model_and_exact_pair_oracle|moving_multicloth_regional_coverage)$'

mkdir -p "$OUT/replays" "$OUT/snapshots" "$OUT/phase16" "$OUT/pair-files" "$OUT/pair-proofs"
echo "[PHASE 17] Replaying original Guard, SAP, Online, regional and optional ML/Bullet on identical material frames"
for material in $MATERIALS; do
  case "$material" in cloth|sponge|cushion|soft_ball|spring|mixed) ;; *)
    echo "Unknown material: $material" >&2; exit 2;; esac
  for regime in $REGIMES; do
    case "$regime" in baseline|compression|shear|impact|rapid|relax) ;; *)
      echo "Unknown regime: $regime" >&2; exit 2;; esac
    stem="${material}-${regime}-n${BUDGET}-f${FRAMES}-seed${SEED}"
    "$BUILD/ccp_material_export" "$OUT/snapshots/${stem}.kmcb" "$material" "$BUDGET" "$FRAMES" "$SEED" "$regime"
    if [[ -n "$MODEL" ]]; then
      "$BUILD/ccp_material_replay" "$OUT/snapshots/${stem}.kmcb" "$OUT/replays/${stem}-evaluated.csv" 0 "$MODEL"
    else
      "$BUILD/ccp_material_replay" "$OUT/snapshots/${stem}.kmcb" "$OUT/replays/${stem}-evaluated.csv" 0
    fi
  done
done

echo "[PHASE 17] Checking actual full pair IDs against independent persistent-grid fingerprints"
for scene in $SCENES; do
  case "$scene" in sparse|slabs|burst|bridge|teleport|all_overlap) ;; *)
    echo "Unknown scene: $scene" >&2; exit 2;; esac
  size="$N"
  if [[ "$scene" == all_overlap ]]; then size=512; fi
  stem="${scene}-n${size}-f${PFRAMES}"
  for threads in $THREADS; do
    case "$threads" in 1|2|4|8|16|32) ;; *)
      echo "Unsupported thread count: $threads" >&2; exit 2;; esac
    "$BUILD/ccp_cbp_pressure_incremental_flat_cpu" "$OUT/phase16/${stem}-threads${threads}.csv" "$scene" "$size" "$PFRAMES" "$SEED" 50000000 25000 12000000 --threads "$threads"
  done
  "$BUILD/ccp_cbp_pressure_emit" "$OUT/phase16/${stem}-emitted.csv" "$scene" "$size" "$PFRAMES" "$SEED" 50000000 25000 64 "$OUT/pair-files/${stem}"
  "$BUILD/ccp_cbp_pressure_incremental_flat_emit" "$OUT/phase16/${stem}-persistent-emitted.csv" "$scene" "$size" "$PFRAMES" "$SEED" 50000000 25000 12000000 64 "$OUT/pair-files/${stem}-persistent"
  python3 scripts/verify_cbp_materialized_pairs.py "$OUT/phase16/${stem}-threads1.csv" "$OUT/phase16/${stem}-emitted.csv" --report "$OUT/pair-proofs/${stem}-PAIR-PROOF.json"
  python3 scripts/verify_cbp_materialized_pairs.py "$OUT/phase16/${stem}-threads1.csv" "$OUT/phase16/${stem}-persistent-emitted.csv" --independent-stream-csv "$OUT/phase16/${stem}-emitted.csv" --report "$OUT/pair-proofs/${stem}-PERSISTENT-PAIR-PROOF.json"
done

python3 scripts/kyoto_research_dashboard.py --materials "$OUT" --phase16 "$OUT/phase16" --warmup 4 --output "$OUT/reports"
echo "[PHASE 17] Complete original-method comparison: $OUT/reports/KYOTO_UNIFIED_EVIDENCE.md"
echo "[PHASE 17] Exact binary pair audit: $OUT/pair-proofs/"
echo "[PHASE 17] This does NOT claim coupled solver or CCD performance."
