#!/usr/bin/env bash
# Full Kyoto material broad-phase reproducibility runner.
# Executed in the foreground on the user's own Apple Silicon Mac.
# Never replaces the Git branch or overwrites raw experimental CSVs.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
MODE="${1:-standard}"
case "$MODE" in
  smoke) SIZES=(64) ; FRAMES=8 ; AUDIT=1 ; CLOTH_FRAMES=16 ; REGIMES=(baseline) ; GPU_REGIMES=(baseline) ;;
  quick) SIZES=(256 1024) ; FRAMES=36 ; AUDIT=4 ; CLOTH_FRAMES=48 ; REGIMES=(baseline impact) ; GPU_REGIMES=(baseline) ;;
  standard) SIZES=(256 1024 4096) ; FRAMES=96 ; AUDIT=8 ; CLOTH_FRAMES=80 ; REGIMES=(baseline impact shear compression) ; GPU_REGIMES=(baseline impact) ;;
  stress) SIZES=(512 2048 8192 16000) ; FRAMES=180 ; AUDIT=12 ; CLOTH_FRAMES=120 ; REGIMES=(baseline impact shear compression rapid relax) ; GPU_REGIMES=(baseline impact compression rapid) ;;
  *) echo "Usage: $0 [smoke|quick|standard|stress]" >&2 ; exit 2 ;;
esac
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RUN="output/kyoto-materials-${STAMP}-${MODE}"
if [ -e "$RUN" ]; then echo "Run directory already exists: $RUN" >&2; exit 2; fi
mkdir -p "$RUN/logs" "$RUN/csv"
finalize_evidence() {
  status=$?
  trap - EXIT
  python3 scripts/package_kyoto_results.py "$RUN" --exit-code "$status" || true
  exit "$status"
}
trap finalize_evidence EXIT
echo "[KYOTO] data directory: $RUN"
{
  echo "timestamp_utc=$STAMP"
  echo "mode=$MODE"
  echo "git_revision=$(git rev-parse HEAD)"
  echo "git_branch=$(git rev-parse --abbrev-ref HEAD)"
  echo "macOS=$(sw_vers -productVersion 2>/dev/null || true)"
  echo "machine=$(uname -m)"
  echo "cpu=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || sysctl -n hw.model 2>/dev/null || true)"
  echo "cpu_cores=$(sysctl -n hw.ncpu 2>/dev/null || true)"
  echo "ram_bytes=$(sysctl -n hw.memsize 2>/dev/null || true)"
  echo "compiler=$(c++ --version | head -1)"
  echo "cmake=$(cmake --version | head -1)"
  echo "gpu:"
  system_profiler SPDisplaysDataType 2>/dev/null || true
  echo "git_status:"
  git status --short
} > "$RUN/system.txt"
echo "[KYOTO] configuring and building Release + tests"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  2>&1 | tee "$RUN/logs/configure.log"
cmake --build build --parallel "${KYOTO_BUILD_JOBS:-4}" \
  2>&1 | tee "$RUN/logs/build.log"
echo "[KYOTO] running correctness regressions"
ctest --test-dir build --output-on-failure \
  -R 'selective_guard_exact_pair_oracle|deformable_material_suite_oracle|moving_multicloth_regional_coverage|scaling_cpu_parallel_oracle|published_3d_broadphase_coverage' \
  2>&1 | tee "$RUN/logs/ctest.log"
echo "[KYOTO] comparing exact current-AABB pairs for six deformable material families"
for n in "${SIZES[@]}"; do
  outfile="$RUN/csv/materials-n${n}.csv"
  echo "[KYOTO] material CPU comparison, target elements=$n, frames=$FRAMES"
  ./build/ccp_material_benchmark "$outfile" all "$n" "$FRAMES" 1234 "$AUDIT" 1 \
    2>&1 | tee "$RUN/logs/materials-n${n}.log"
done
CSVFILES=()
for n in "${SIZES[@]}"; do CSVFILES+=("$RUN/csv/materials-n${n}.csv"); done
for regime in "${REGIMES[@]}"; do
  if [ "$regime" = baseline ]; then continue; fi
  case "$MODE" in smoke) extraN=64; extraFrames=8;; quick) extraN=256; extraFrames=36;;
    standard) extraN=1024; extraFrames=96;; stress) extraN=2048; extraFrames=180;; esac
  outfile="$RUN/csv/materials-${regime}-n${extraN}.csv"
  echo "[KYOTO] deformation stress regime=$regime n=$extraN"
  ./build/ccp_material_benchmark "$outfile" all "$extraN" "$extraFrames" 1234 "$AUDIT" 1 "$regime" \
    2>&1 | tee "$RUN/logs/materials-${regime}.log"
  CSVFILES+=("$outfile")
done
python3 scripts/summarize_material_suite.py "${CSVFILES[@]}" \
  --output "$RUN/MATERIAL_RESULTS.md" > "$RUN/logs/material-summary.log"
echo "[KYOTO] coupled cloth - original Online controller vs Global Guard / SAP"
./build/ccp_multicloth_benchmark "$RUN/csv/coupled-2-obstacles.csv" "$CLOTH_FRAMES" 2 \
  2>&1 | tee "$RUN/logs/coupled-2-obstacles.log"
./build/ccp_multicloth_benchmark "$RUN/csv/coupled-5-obstacles.csv" "$CLOTH_FRAMES" 5 \
  2>&1 | tee "$RUN/logs/coupled-5-obstacles.log"
TRACE_FRAMES=$(( CLOTH_FRAMES < 60 ? 60 : CLOTH_FRAMES ))
./build/ccp_online_trace "$RUN/csv/online-original-trace.csv" "$TRACE_FRAMES" \
  2>&1 | tee "$RUN/logs/online-trace.log"
echo "[KYOTO] exact material snapshots shared between CPU and Metal"
case "$MODE" in
 smoke) REPLAY_BUDGET=64 ; REPLAY_FRAMES=8 ;;
 quick) REPLAY_BUDGET=256 ; REPLAY_FRAMES=30 ;;
 standard) REPLAY_BUDGET=1024 ; REPLAY_FRAMES=64 ;;
 stress) REPLAY_BUDGET=4096 ; REPLAY_FRAMES=120 ;;
esac
mkdir -p "$RUN/snapshots" "$RUN/verification"
for regime in "${GPU_REGIMES[@]}"; do
for mat in cloth sponge cushion soft_ball spring mixed; do
  snapshot="$RUN/snapshots/${mat}-${regime}.kmcb"
  cpu="$RUN/csv/actual-${mat}-${regime}-cpu.csv"
  echo "[KYOTO] serializing $mat Float32 AABB trajectories"
  ./build/ccp_material_export "$snapshot" "$mat" "$REPLAY_BUDGET" "$REPLAY_FRAMES" 1234 "$regime" \
    2>&1 | tee "$RUN/logs/export-${mat}-${regime}.log"
  ./build/ccp_material_replay "$snapshot" "$cpu" 1 \
    2>&1 | tee "$RUN/logs/replay-${mat}-${regime}-cpu.log"
  if [ -x ./build/ccp_scaling_metal ]; then
    for backend in sap region online; do
      gpu="$RUN/csv/actual-${mat}-${regime}-metal-${backend}.csv"
      ./build/ccp_scaling_metal --replay "$snapshot" "$gpu" "$backend" \
        2>&1 | tee "$RUN/logs/replay-${mat}-${regime}-gpu-${backend}.log"
      python3 scripts/verify_material_cpu_gpu.py "$cpu" "$gpu" \
        --output "$RUN/verification/${mat}-${regime}-cpu-metal-${backend}.md" \
        2>&1 | tee "$RUN/logs/verify-${mat}-${regime}-gpu-${backend}.log"
    done
  else
    echo "No Metal GPU executable: CPU-only for $mat" | tee "$RUN/logs/metal-unavailable-${mat}-${regime}.log"
  fi
done
done
echo "[KYOTO] cross-backend reference tests (synthetic GPU SAP/complete oracle controls)"
if [ -x ./build/ccp_scaling_metal ]; then
  ./build/ccp_scaling_metal "$RUN/csv/gpu-sap-hotspot.csv" 512 32 hotspot 1234 sap \
    2>&1 | tee "$RUN/logs/gpu-sap.log"
  ./build/ccp_scaling_metal "$RUN/csv/gpu-exhaustive-hotspot.csv" 512 32 hotspot 1234 exhaustive \
    2>&1 | tee "$RUN/logs/gpu-exhaustive.log"
else
  echo "Metal target unavailable. No GPU timing claim." | tee "$RUN/logs/gpu-unavailable.log"
fi
