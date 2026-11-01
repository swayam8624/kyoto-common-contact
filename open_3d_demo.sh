#!/bin/sh
set -eu
TASK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$TASK_DIR/build/ccp_native.app/Contents/MacOS/ccp_native"
# Build every launch, because existing binaries can be stale after edits.
# For the final frozen demo only, CCP_SKIP_BUILD=1 skips rebuilding.
if [ "${CCP_SKIP_BUILD:-0}" != "1" ]; then
    cmake -S "$TASK_DIR" -B "$TASK_DIR/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    cmake --build "$TASK_DIR/build" --target ccp_native --parallel
fi
if [ ! -x "$APP" ]; then
    echo "Native app missing: $APP" >&2
    exit 1
fi
exec "$APP" "$@"
