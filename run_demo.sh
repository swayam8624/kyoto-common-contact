#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$ROOT/build"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$BUILD_DIR" --parallel

if [ -x /Users/swayamsingal/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node ]; then
  NODE_BIN=/Users/swayamsingal/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node
else
  NODE_BIN=$(command -v node)
fi
cd "$ROOT"
exec "$NODE_BIN" viewer/server.mjs
