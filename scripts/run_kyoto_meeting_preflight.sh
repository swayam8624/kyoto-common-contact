#!/usr/bin/env bash
# Kyoto meeting preflight, 24 Nov 2026. Run locally on macOS/Linux.
# Fail closed on every native correctness/compilation error. Native
# scientific outputs only; nothing is simulated by this shell script.
set -euo pipefail

MODE="${1:-core}"
if [[ "$MODE" != "core" && "$MODE" != "ipc" ]]; then
  echo "usage: bash scripts/run_kyoto_meeting_preflight.sh [core|ipc]" >&2
  exit 2
fi
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUTPUT="results/kyoto-meeting-2026-11-24"
mkdir -p "$OUTPUT"

{
  echo "Kyoto University research preflight, 24 November 2026"
  echo "Mode: $MODE"
  echo "UTC: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "Branch: $(git branch --show-current)"
  echo "Commit: $(git rev-parse HEAD)"
  echo "Dirty files:"
  git status --porcelain
  echo "Operating system: $(uname -a)"
  echo "CMake: $(cmake --version | head -n 1)"
  echo "C++ compiler: ${CXX:-default from CMake}"
} > "$OUTPUT/provenance.txt"

echo "=== 1 / Build real C++23 FEM and independent hierarchy checks ==="
cmake -S . -B build-meeting-core -DCMAKE_BUILD_TYPE=Release
cmake --build build-meeting-core --parallel 4 --target \
  kyoto_refit_feature_hierarchy_tests \
  kyoto_fem_feature_snapshots_tests \
  kyoto_pass13_reference_fem_benchmark

echo "=== 2 / Independent source geometry and candidate parity ==="
ctest --test-dir build-meeting-core --output-on-failure -R \
  'maintained_refit_hierarchy_VF_EE_oracle_and_invalidation|identical_real_FEM_feature_source_and_refit_contract' \
  | tee "$OUTPUT/core_tests.txt"

echo "=== 3 / Original 3D physically integrated FEM snapshots ==="
mkdir -p "$OUTPUT/core-source" "$OUTPUT/core-figures"
./build-meeting-core/kyoto_pass13_reference_fem_benchmark \
  "$OUTPUT/core-source" | tee "$OUTPUT/core_console.txt"
python3 scripts/report_pass13_fem_baselines.py \
  --input "$OUTPUT/core-source/native_fem_reference_broadphase.csv" \
  --output "$OUTPUT/core-figures" | tee "$OUTPUT/core_report.txt"

if [[ "$MODE" == "ipc" ]]; then
  echo "=== 4 / Build pinned official upstream IPC Toolkit LBVH ==="
  cmake -S . -B build-meeting-ipc -DCMAKE_BUILD_TYPE=Release \
    -DKYOTO_WITH_OFFICIAL_IPC=ON -DIPC_TOOLKIT_WITH_SIMD=OFF
  cmake --build build-meeting-ipc --parallel 4 --target \
    kyoto_pass15_official_ipc_fem_benchmark \
    kyoto_official_ipc_lbvh_tests \
    kyoto_refit_feature_hierarchy_tests \
    kyoto_fem_feature_snapshots_tests

  echo "=== 5 / Official IPC and original FEM feature safety gates ==="
  ctest --test-dir build-meeting-ipc --output-on-failure -R \
    'pinned_official_ipc_lbvh_feature_coverage|maintained_refit_hierarchy_VF_EE_oracle_and_invalidation|identical_real_FEM_feature_source_and_refit_contract' \
    | tee "$OUTPUT/ipc_tests.txt"

  echo "=== 6 / Matched upstream IPC and maintained hierarchy original evidence ==="
  mkdir -p "$OUTPUT/ipc-source" "$OUTPUT/ipc-figures"
  ./build-meeting-ipc/kyoto_pass15_official_ipc_fem_benchmark \
    "$OUTPUT/ipc-source" | tee "$OUTPUT/ipc_console.txt"
  python3 scripts/report_pass15_official_ipc.py \
    --input "$OUTPUT/ipc-source/official_ipc_fem_paired.csv" \
    --output "$OUTPUT/ipc-figures" | tee "$OUTPUT/ipc_report.txt"
fi

echo "PASS: requested local meeting preflight completed on this machine."
echo "Original evidence and provenance: $OUTPUT"
echo "Remember: candidate-only timings are NOT complete solver speedups."
