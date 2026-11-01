#!/bin/sh
# New research-first 3D demo: REAL coupled multicloth physics, two oscillating
# spherical obstacles and live regional broad-phase instrumentation.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# The ONLINE policy is the research default; the previous fixed policies
# remain selectable with Q or T for direct comparisons.
exec "$ROOT/open_3d_demo.sh" --multicloth --online "$@"
