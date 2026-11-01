#!/bin/sh
set -eu
TASK_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$TASK_DIR/build/ccp_native.app/Contents/MacOS/ccp_native"
if [ ! -x "$APP" ]; then
    cmake -S "$TASK_DIR" -B "$TASK_DIR/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$TASK_DIR/build" --target ccp_native --parallel
fi
exec "$APP" "$@"
