#!/bin/sh
set -eu
cd -P "$(dirname "$0")"
cmake -S . -B out/play -DCMAKE_BUILD_TYPE=Release
cmake --build out/play --config Release --target raycast-2d
[ "${1:-}" != --build-only ] || exit 0
if [ "$#" -eq 0 ]; then set -- --config assets/game.cfg; fi
if [ -x out/play/Release/raycast-2d ]; then
    exec ./out/play/Release/raycast-2d "$@"
fi
exec ./out/play/raycast-2d "$@"
