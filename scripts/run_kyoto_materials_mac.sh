#!/usr/bin/env bash
# Full Kyoto material broad-phase reproducibility runner.
# Executed in the foreground on the user's own Apple Silicon Mac.
# Never replaces the Git branch or overwrites raw experimental CSVs.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
MODE="${1:-standard}"
case "$MODE" in
  smoke) SIZES=(64) ; FRAMES=8 ; AUDIT=1 ; CLOTH_FRAMES=16 ;;
  quick) SIZES=(256 1024) ; FRAMES=36 ; AUDIT=4 ; CLOTH_FRAMES=48 ;;
  standard) SIZES=(256 1024 4096) ; FRAMES=96 ; AUDIT=8 ; CLOTH_FRAMES=80 ;;
  stress) SIZES=(512 2048 8192 16000) ; FRAMES=180 ; AUDIT=12 ; CLOTH_FRAMES=120 ;;
  *) echo "Usage: $0 [smoke|quick|standard|stress]" >&2 ; exit 2 ;;
esac
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RUN="output/kyoto-materials-${STAMP}-${MODE}"
if [ -e "$RUN" ]; then echo "Run directory already exists: $RUN" >&2; exit 2; fi
mkdir -p "$RUN/logs" "$RUN/csv"
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
python3 scripts/summarize_material_suite.py "${CSVFILES[@]}" \
  --output "$RUN/MATERIAL_RESULTS.md" > "$RUN/logs/material-summary.log"
echo "[KYOTO] coupled cloth - original Online controller vs Global Guard / SAP"
./build/ccp_multicloth_benchmark "$RUN/csv/coupled-2-obstacles.csv" "$CLOTH_FRAMES" 2 \
  2>&1 | tee "$RUN/logs/coupled-2-obstacles.log"
./build/ccp_multicloth_benchmark "$RUN/csv/coupled-5-obstacles.csv" "$CLOTH_FRAMES" 5 \
  2>&1 | tee "$RUN/logs/coupled-5-obstacles.log"
./build/ccp_online_trace "$RUN/csv/online-original-trace.csv" "$CLOTH_FRAMES" \
  2>&1 | tee "$RUN/logs/online-trace.log"
echo "[KYOTO] cross-backend reference tests (GPU is SAP, not our online controller)"
if [ -x ./build/ccp_scaling_metal ]; then
  ./build/ccp_scaling_metal "$RUN/csv/gpu-sap-hotspot.csv" 512 32 hotspot 1234 sap \
    2>&1 | tee "$RUN/logs/gpu-sap.log"
  ./build/ccp_scaling_metal "$RUN/csv/gpu-exhaustive-hotspot.csv" 512 32 hotspot 1234 exhaustive \
    2>&1 | tee "$RUN/logs/gpu-exhaustive.log"
else
  echo "Metal target unavailable. No GPU timing claim." | tee "$RUN/logs/gpu-unavailable.log"
fi
python3 - "$RUN" <<'PY'
import hashlib, json, pathlib, sys, zipfile
root=pathlib.Path(sys.argv[1])
files=sorted(p for p in root.rglob("*") if p.is_file())
manifest={"directory":str(root),"files":{
 str(p.relative_to(root)):{"size":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()}
 for p in files}}
(root/"SHA256_MANIFEST.json").write_text(json.dumps(manifest,indent=2)+"\n")
archive=root.with_suffix(".zip")
with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(root.rglob("*")):
  if p.is_file():z.write(p,p.relative_to(root.parent))
print(f"[KYOTO] COMPLETE archive: {archive}")
print("[KYOTO] Upload the ZIP; it contains raw CSV, full verbose logs, validation and hardware metadata.")
PY
