#!/bin/zsh
# The go-kart (kart-rig.js, kart.js), serverless on world.json's ground:
#   tests-manual/physics-baseline/run-kart.sh [path/to/Darkride.app] [results dir]
#   KART_FLOOR_FRICTION=0.5 tests-manual/physics-baseline/run-kart.sh ...   (the ground's grip; 0.9 by default)
#   KART_OVERRIDE='Kart.SPEC.maxSteerAngle = 40;' ...   (JavaScript run after kart-rig.js: a tuning try)
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
{ [ -n "$KART_FLOOR_FRICTION" ] && echo "var KART_CONFIG = { floorFriction: $KART_FLOOR_FRICTION };"
  cat "$D/kart-rig.js"; [ -n "$KART_OVERRIDE" ] && echo "$KART_OVERRIDE"; cat "$D/kart.js"; } > "$R/kart-test.js"
echo "client: $BIN"
echo "results: $R"
SESSION=${SESSION:-physics}
. "$D/harness.sh"
client kart --url "file://$D/world.json" --testScript "$R/kart-test.js" quitWhenFinished --testResultsLocation "$R" || true
grep -a "KART" "$R/kart.log" | sed 's/.*KART/KART/' || { echo "no summary in $R/kart.log" >&2; exit 1; }
