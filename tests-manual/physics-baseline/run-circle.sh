#!/bin/zsh
# Running in circles (circle.js, GitHub #15): one walled-off client, serverless on world.json, no mouse or keyboard
# (a controller mapping holds sprint, forward and turn). Prints the CIRCLE lines: per phase the laps, the frame rate,
# the radius and how far the circle's centre moves per lap.
#   tests-manual/physics-baseline/run-circle.sh [path/to/Darkride.app] [results dir]
#
# Copyright 2026 Darkride.
#
# Distributed under the MIT License.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${${1:-"$D/../../build/interface/Darkride.app"}:A}
R=${2:-"${TMPDIR:-/tmp}/circle"}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R"   # Test.saveObject writes into the results location only when it exists
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}
echo "client: $BIN"; echo "results: $R"
SESSION=${SESSION:-physics}
CLIENT_TIMEOUT=${CLIENT_TIMEOUT:-480}
. "$D/harness.sh"
client circle --url "file://$D/world.json" --testScript "$D/circle.js" quitWhenFinished --testResultsLocation "$R" || true
grep -a "CIRCLE" "$R/circle.log" | sed 's/.*CIRCLE/CIRCLE/' || { echo "no CIRCLE lines in $R/circle.log" >&2; exit 1; }
