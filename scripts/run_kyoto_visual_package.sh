#!/usr/bin/env bash
# Honest 6-material x 6-regime GIF/MP4 montage and all 7 real physics policies.
# A short correctness gate; NO exhaustive multi-hour parameter search.
# bash scripts/run_kyoto_visual_package.sh [OUTPUT] [OPTIONAL_ML_MODEL]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUTPUT="${1:-output/kyoto-final-showcase}"
MODEL="${2:-}"
BUILD="${KYOTO_SHOWCASE_BUILD:-build/kyoto-visual-release}"
FRAMES="${KYOTO_SHOWCASE_FRAMES:-48}"
SOLVER_FRAMES="${KYOTO_SHOWCASE_SOLVER_FRAMES:-42}"
BUDGET="${KYOTO_SHOWCASE_BUDGET:-128}"
SEED="${KYOTO_SHOWCASE_SEED:-31337}"

if [[ -e "$OUTPUT" ]]; then
  echo "Refusing to overwrite existing showcase: $OUTPUT" >&2
  echo "Choose a new output directory; original evidence remains unchanged." >&2
  exit 2
fi
if (( FRAMES<24 || FRAMES>180 || SOLVER_FRAMES<12 || SOLVER_FRAMES>180 )); then
  echo "Invalid frame count; replay 24..180, coupled solver 12..180." >&2;exit 2
fi
if [[ "$BUDGET" != 128 && "$BUDGET" != 512 && "$BUDGET" != 2048 ]]; then
  echo "Budget must be 128, 512 or 2048." >&2;exit 2
fi
if ! python3 -c "import PIL" >/dev/null 2>&1; then
  echo "Install Pillow: python3 -m pip install Pillow" >&2;exit 2
fi
if [[ -n "$MODEL" && ! -r "$MODEL" ]]; then
  echo "ML model not readable: $MODEL" >&2;exit 2
fi

cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$BUILD" --parallel 4 --target \
  ccp_material_export ccp_material_replay \
  ccp_visual_multicloth_export ccp_final_broadphase_api_tests
ctest --test-dir "$BUILD" --output-on-failure \
  -R final_broadphase_public_api_exact_pair_oracle

mkdir -p "$OUTPUT/snapshots" "$OUTPUT/replays" \
         "$OUTPUT/meshes" "$OUTPUT/reports" "$OUTPUT/videos"
echo "[SHOWCASE] Exporting 36 real material/regime sequences, budget $BUDGET"
for material in cloth sponge cushion soft_ball spring mixed; do
  for regime in baseline compression shear impact rapid relax; do
    stem="actual-${material}-${regime}-n${BUDGET}-seed${SEED}"
    snapshot="$OUTPUT/snapshots/$stem.kmcb"
    csv="$OUTPUT/replays/$stem-evaluated.csv"
    "$BUILD/ccp_material_export" "$snapshot" "$material" "$BUDGET" \
       "$FRAMES" "$SEED" "$regime" > /dev/null
    if [[ -n "$MODEL" ]]; then
      "$BUILD/ccp_material_replay" "$snapshot" "$csv" 0 "$MODEL" > /dev/null
    else
      "$BUILD/ccp_material_replay" "$snapshot" "$csv" 0 > /dev/null
    fi
  done
done
python3 scripts/render_kyoto_visuals.py aabb \
  --run-dir "$OUTPUT" --output "$OUTPUT/videos/all-materials-six-regimes" \
  --budget "$BUDGET" --seed "$SEED" --frames-per-regime 5 --fps 6

echo "[SHOWCASE] Exporting coupled cloth/sphere solver meshes for all 7 policies"
"$BUILD/ccp_visual_multicloth_export" "$OUTPUT/meshes" \
  "$SOLVER_FRAMES" 2 2
python3 benchmarks/quality_report.py "$OUTPUT/meshes" --output "$OUTPUT/reports"
python3 scripts/render_kyoto_visuals.py solver \
  --input-dir "$OUTPUT/meshes" \
  --output "$OUTPUT/videos/all-seven-solver-policies" \
  --stride 2 --fps 8
python3 benchmarks/score_replay.py \
  "$OUTPUT/replays/actual-cloth-baseline-n${BUDGET}-seed${SEED}-evaluated.csv" \
  --warmup 8 --output "$OUTPUT/reports"

KYOTO_OUT="$OUTPUT" KYOTO_SEED="$SEED" KYOTO_BUDGET="$BUDGET" \
  python3 - <<'PY'
import hashlib,json,os,platform,subprocess
from pathlib import Path
out=Path(os.environ["KYOTO_OUT"])
def digest(path):
    h=hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""):h.update(block)
    return h.hexdigest()
files=sorted(out.glob("snapshots/*.kmcb"))
manifest={
 "schema":1,"purpose":"frozen exact CPU AABB replay + verified coupled-solver visuals",
 "git_sha":subprocess.check_output(["git","rev-parse","HEAD"],text=True).strip(),
 "system":platform.platform(),"machine":platform.machine(),
 "seed":int(os.environ["KYOTO_SEED"]),"budget":int(os.environ["KYOTO_BUDGET"]),
 "snapshot_sha256":{p.name:digest(p) for p in files},
 "snapshot_count":len(files),
 "limitations":[
   "Same AABB snapshots across material replay methods",
   "When candidate pairs are exact, no broad-phase-specific folding improvements exist",
   "Seven coupled-solver algorithms are distinct from final standalone V4",
   "Exact AABB pair parity is not continuous collision safety",
   "Audited mesh exporter timings include diagnostic overhead",
 ],
}
(out/"SHOWCASE_MANIFEST.json").write_text(json.dumps(manifest,indent=2)+"\n")
print("[SHOWCASE] Manifest:",out/"SHOWCASE_MANIFEST.json")
PY
echo "[SHOWCASE] DONE: all-materials-six-regimes.gif, all-seven-solver-policies.gif, reports"
echo "[SHOWCASE] Matching MP4 files are generated when ffmpeg is installed"
