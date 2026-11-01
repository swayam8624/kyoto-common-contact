#!/bin/sh
# New research-first 3D demo: REAL coupled multicloth physics, two oscillating
# spherical obstacles and live regional broad-phase instrumentation.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$ROOT/open_3d_demo.sh" --multicloth "$@"
