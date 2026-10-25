#!/bin/zsh
# The go-kart (kart-rig.js, kart.js), serverless on world.json's ground:
#   tests-manual/physics-baseline/run-kart.sh [path/to/Darkride.app] [results dir]
# One walled-off client (harness.sh's client). Prints the KART lines; the samples are in <results>/kart.json.
#
# Copyright 2026 Darkride.
#
# Distributed under the MIT License.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${${1:-"$D/../../build/interface/Darkride.app"}:A}
R=${2:-"${TMPDIR:-/tmp}/kart"}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R"
cat "$D/kart-rig.js" "$D/kart.js" > "$R/kart-test.js"
echo "client: $BIN"
echo "results: $R"
SESSION=${SESSION:-physics}
. "$D/harness.sh"
client kart --url "file://$D/world.json" --testScript "$R/kart-test.js" quitWhenFinished --testResultsLocation "$R" || true
grep -a "KART" "$R/kart.log" | sed 's/.*KART/KART/' || { echo "no summary in $R/kart.log" >&2; exit 1; }
