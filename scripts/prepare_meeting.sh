#!/bin/sh
# Reproducible Kyoto meeting readiness: fail before printing a success report.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
mkdir -p output/native
./build/ccp_live_benchmark output/native/live_benchmark-postfix.csv | tee output/native/benchmark-postfix-summary.txt
python3 scripts/summarize_benchmark.py output/native/live_benchmark-postfix.csv output/native/benchmark-postfix.svg docs/MEETING_RESULTS.md
printf '\nMeeting verification completed; read docs/MEETING_RESULTS.md.\n'
if [ -x build/ccp_native.app/Contents/MacOS/ccp_native ]; then
  printf 'Native viewer: ./open_3d_demo.sh\n'
fi
printf 'Legacy islands/CCD viewer: ./run_demo.sh\n'
