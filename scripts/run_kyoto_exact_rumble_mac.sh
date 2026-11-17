#!/usr/bin/env bash
# Non-destructive exact AABB broad-phase deathmatch.
# Usage: bash scripts/run_kyoto_exact_rumble_mac.sh MODEL.mlpc [OUTPUT]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: bash scripts/run_kyoto_exact_rumble_mac.sh /absolute/path/controller.mlpc [output]" >&2
  exit 2
fi
MODEL="$1"
OUTPUT="${2:-output/kyoto-exact-rumble-v5-full}"
BUILD="${KYOTO_RUMBLE_BUILD:-build/kyoto-rumble-macos}"
REPEATS="${KYOTO_RUMBLE_REPEATS:-3}"
FRAMES="${KYOTO_RUMBLE_FRAMES:-160}"
WARMUP="${KYOTO_RUMBLE_WARMUP:-16}"
MAX_EXPERIMENTS="${KYOTO_RUMBLE_MAX_EXPERIMENTS:-0}"
RESUME=()
if [[ "${KYOTO_RUMBLE_RESUME:-0}" == "1" ]]; then RESUME=(--resume); fi

# An isolated worktree protects V4 and its trained neural model.
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCCP_REQUIRE_BULLET=ON
cmake --build "$BUILD" --target ccp_material_export ccp_material_replay --parallel 4
ctest --test-dir "$BUILD" --output-on-failure -R learned_cpp_model_and_exact_pair_oracle
python3 -m unittest scripts.test_ablation_preflight scripts.test_rumble_analysis -v
python3 scripts/test_rumble_adversarial.py "$BUILD/ccp_material_replay"
c++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/kinetic_certificates_tests.cpp -o "$BUILD/ccp_kinetic_certificates_tests"
"$BUILD/ccp_kinetic_certificates_tests"

echo "[RUMBLE] Model: $MODEL"
echo "[RUMBLE] Fresh output: $OUTPUT"
echo "[RUMBLE] 6 materials x 6 motion regimes x 3 budgets x 8 seeds x $REPEATS repeats"
echo "[RUMBLE] Frames per replay: $FRAMES; warmup $WARMUP; max experiments: $MAX_EXPERIMENTS (0 = all)"
python3 scripts/run_kyoto_controller_ablation.py \
  --model "$MODEL" \
  --build "$BUILD" \
  --output "$OUTPUT" \
  --seeds 3203 3217 3251 3299 3313 3343 3371 3407 \
  --sampling balanced \
  --sample-seed 20261118 \
  --order-seed 20261119 \
  --repeats "$REPEATS" \
  --frames "$FRAMES" \
  --warmup "$WARMUP" \
  --max-experiments "$MAX_EXPERIMENTS" \
  --timeout 900 \
  --bootstrap 2000 \
  "${RESUME[@]}"

echo "[RUMBLE] Finished: $OUTPUT/RUMBLE_PATTERNS.md and RUMBLE_PATTERNS.json"
