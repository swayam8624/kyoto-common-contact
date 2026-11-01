#!/bin/bash
# Reproducible Kyoto meeting readiness: fail before printing a success report.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
# The comparative study must use the real Bullet 3D broadphase, never a fake
# local stand-in. Install with: brew install bullet (macOS) or apt libbullet-dev.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCCP_REQUIRE_BULLET=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
mkdir -p output/native
./build/ccp_cloth_stability_probe 480 | tee output/native/cloth-stability.txt
./build/ccp_live_benchmark output/native/live_benchmark-postfix.csv | tee output/native/benchmark-postfix-summary.txt
python3 scripts/summarize_benchmark.py output/native/live_benchmark-postfix.csv output/native/benchmark-postfix.svg docs/MEETING_RESULTS.md
python3 scripts/summarize_pipeline.py output/native/live_benchmark-postfix.csv output/native/cascade.svg
python3 scripts/export_supporting_replays.py
# The actual multi-cloth comparison rejects any changed physical trajectory.
./build/ccp_multicloth_benchmark output/native/multicloth-real.csv 48
python3 scripts/summarize_multicloth.py output/native/multicloth-real.csv output/native/multicloth-real.svg docs/MULTICLOTH_RESULTS.md
./build/ccp_online_trace output/native/online-controller-trace.csv 160
python3 scripts/summarize_online_trace.py output/native/online-controller-trace.csv output/native/online-controller-trace.svg docs/ONLINE_CONTROLLER_RESULTS.md
./build/ccp_patch_sweep output/native/patch-sweep.csv 48
python3 scripts/summarize_patch_sweep.py output/native/patch-sweep.csv output/native/patch-sweep.svg docs/PATCH_CALIBRATION.md
./build/ccp_broadphase_leaderboard output/native/broadphase-leaderboard.csv
python3 scripts/summarize_leaderboard.py output/native/broadphase-leaderboard.csv output/native/broadphase-leaderboard.svg docs/BROADPHASE_RESULTS.md
printf '\nMeeting verification completed; read docs/MEETING_RESULTS.md.\n'
if [ -x build/ccp_native.app/Contents/MacOS/ccp_native ]; then
  printf 'Native viewer: ./open_3d_demo.sh\n'
fi
printf 'Legacy islands/CCD viewer: ./run_demo.sh\n'
