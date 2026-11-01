#!/bin/zsh
# Camera collision for the third-person camera (camera-collision.js), serverless on world.json's ground:
#   tests-manual/physics-baseline/run-camera.sh [path/to/Darkride.app] [results dir]
# One walled-off client (harness.sh's client: its own settings and app data in <results>/camera-home, no sign-in, the
# machine lock, the small corner window, the TEST title). Prints the CAMERA-COLLISION lines; the samples are in
# <results>/camera-collision.json, the pictures (the wall case with the option on and off) in <results>/camera-home/snaps.
#
# Copyright 2026 Darkride.
#
# Distributed under the MIT License.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${${1:-"$D/../../build/interface/Darkride.app"}:A}   # absolute: open -a takes no relative path
R=${2:-"${TMPDIR:-/tmp}/camera-collision"}
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
mkdir -p "$R"
echo "client: $BIN"
echo "results: $R"
. "${HARNESS:-$D/harness.sh}"   # HARNESS: another harness.sh, say a branch's not yet in main
# CAMERA_AVATAR_URL=<an .fst> wears that avatar (a big head or hood reaches walls the default avatar does not)
{ [ -n "$CAMERA_AVATAR_URL" ] && echo "var CAMERA_CONFIG = { avatarURL: \"$CAMERA_AVATAR_URL\" };"; cat "$D/camera-collision.js"; } > "$R/camera-test.js"
# CAMERA_FOV=<degrees> starts the client with that field of view (Caity's is 116; the default 45)
if [ -n "$CAMERA_FOV" ]; then
    wall camera
    echo "{\"snapshotsLocation\": \"$R/camera-home/snaps\", \"WindowGeometry\": \"$CLIENT_WINDOW\", \"fieldOfView\": $CAMERA_FOV}" > "$R/camera-home/config/Tivoli Cloud VR - dev/Interface.json"
fi
client camera --url "file://$D/world.json" --testScript "$R/camera-test.js" quitWhenFinished --testResultsLocation "$R" || true
grep -a "CAMERA-COLLISION" "$R/camera.log" | sed 's/.*CAMERA-COLLISION/CAMERA-COLLISION/' || { echo "no summary in $R/camera.log" >&2; exit 1; }
ls "$R"/camera-home/snaps 2>/dev/null
