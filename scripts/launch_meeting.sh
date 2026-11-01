#!/bin/sh
# Kyoto meeting launch menu; choose one reproducible, clearly labeled demo.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mode="${1:-native}"
case "$mode" in
  native|cloth)
    exec ./open_3d_demo.sh
    ;;
  verify|check)
    exec ./scripts/prepare_meeting.sh
    ;;
  islands|spheres|ccd)
    ./run_demo.sh &
    PID=$!
    trap 'kill "$PID" 2>/dev/null || true' EXIT INT TERM
    sleep 2
    if command -v open >/dev/null 2>&1; then
      open http://127.0.0.1:4173/
    else
      echo 'Open http://127.0.0.1:4173/ in a browser'
    fi
    wait "$PID"
    ;;
  offline)
    if command -v open >/dev/null 2>&1; then
      exec open output/Common_Broad_Phase_Demo.html
    else
      echo "Open output/Common_Broad_Phase_Demo.html in a browser"
    fi
    ;;
  video)
    if command -v open >/dev/null 2>&1; then
      exec open output/research3d/Common_Broad_Phase_3D.mp4
    else
      echo "Open output/research3d/Common_Broad_Phase_3D.mp4"
    fi
    ;;
  *)
    echo "usage: $0 [native|islands|ccd|offline|video|verify]" >&2
    exit 2
    ;;
esac
