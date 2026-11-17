#!/usr/bin/env bash
# V6 isolated exact certified-interaction hierarchy experiment.
# Usage: bash scripts/run_kyoto_unified_v6_mac.sh MODEL.mlpc [OUTPUT]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: bash scripts/run_kyoto_unified_v6_mac.sh /absolute/path/controller.mlpc [output]" >&2
  exit 2
fi
MODEL="$1"
OUTPUT="${2:-output/kyoto-v6-unified-fresh}"
BUILD="${KYOTO_V6_BUILD:-build/kyoto-rumble-macos}"
REPEATS="${KYOTO_V6_REPEATS:-2}"
FRAMES="${KYOTO_V6_FRAMES:-160}"
WARMUP="${KYOTO_V6_WARMUP:-16}"
MAX_EXPERIMENTS="${KYOTO_V6_MAX_EXPERIMENTS:-36}"
# The argument array is always non-empty; macOS Bash 3.2 + set -u
# treats expansion of an *empty* array as an unbound variable.
# Build a single complete argv, and append --resume only when explicitly set.

# An isolated worktree protects V4 and its trained neural model.
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCCP_REQUIRE_BULLET=ON
cmake --build "$BUILD" --target ccp_material_export ccp_material_replay --parallel 4
ctest --test-dir "$BUILD" --output-on-failure -R learned_cpp_model_and_exact_pair_oracle
python3 -m unittest scripts.test_ablation_preflight scripts.test_rumble_analysis -v
python3 scripts/test_rumble_adversarial.py "$BUILD/ccp_material_replay"
python3 scripts/test_rumble_endtoend.py "$BUILD"
c++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/kinetic_certificates_tests.cpp -o "$BUILD/ccp_kinetic_certificates_tests"
"$BUILD/ccp_kinetic_certificates_tests"

echo "[RUMBLE] Model: $MODEL"
echo "[RUMBLE] Fresh output: $OUTPUT"
echo "[RUMBLE] 6 materials x 6 motion regimes x 3 budgets x 8 NEW seeds x $REPEATS repeats"
echo "[RUMBLE] Frames per replay: $FRAMES; warmup $WARMUP; max experiments: $MAX_EXPERIMENTS (0 = all)"
ABLATION_ARGS=(
  --model "$MODEL"
  --build "$BUILD"
  --output "$OUTPUT"
  --seeds 4103 4111 4127 4153 4177 4201 4217 4231
  --sampling balanced
  --sample-seed 20261120
  --order-seed 20261121
  --repeats "$REPEATS"
  --frames "$FRAMES"
  --warmup "$WARMUP"
  --max-experiments "$MAX_EXPERIMENTS"
  --timeout 900
  --bootstrap 2000
)
if [[ "${KYOTO_V6_RESUME:-0}" == "1" ]]; then
  ABLATION_ARGS+=(--resume)
fi
python3 scripts/run_kyoto_controller_ablation.py "${ABLATION_ARGS[@]}"

echo "[RUMBLE] Finished: $OUTPUT/RUMBLE_PATTERNS.md and RUMBLE_PATTERNS.json"
