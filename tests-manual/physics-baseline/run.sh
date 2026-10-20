#!/bin/zsh
# Run the physics baseline (PHYSICS.md round 0) against a built client, serverless.
#   tests-manual/physics-baseline/run.sh [path/to/Darkride.app] [results dir] [plain|frame|track]
# "frame" (round 2) makes the cabin a moving reference frame; "track" (round 4) rides a Track entity (track.js,
# PHYSICS-TRACK lines, samples in physics-track.json). Prints the summary lines;
# the samples land in <results dir>/physics-baseline.json. The client is walled off, see harness.sh.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/physics-baseline"}
MODE=${3:-plain}
SCRIPT="$D/baseline.js"
[ "$MODE" = "frame" ] && SCRIPT="$D/baseline-frame.js"
[ "$MODE" = "track" ] && SCRIPT="$D/track.js"
mkdir -p "$R"
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
. "$D/harness.sh"
echo "client: $BIN"
echo "results: $R"
client client --url "file://$D/world.json" --testScript "$SCRIPT" --testResultsLocation "$R" || true
grep "PHYSICS-BASELINE\|PHYSICS-TRACK" "$R/client.log" || { echo "no summary in $R/client.log" >&2; exit 1; }
