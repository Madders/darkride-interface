#!/bin/sh
# Check the spring bones of an imported avatar with a built client, serverless: load the avatar, stand, walk, stop,
# spin, settle, and record every chain joint each script frame. Prints SPRING-CHECK lines (per chain and phase the
# tip's motion against its root, and the single-joint chains' swing) and saves the samples with Test.saveObject.
#   tests-manual/spring-bones/run.sh [path/to/Darkride.app] [results dir] avatar.fst   (or the .fst in SPRING_CHECK_AVATAR)
# The client is walled off (its own XDG_CONFIG_HOME and TIVOLI_APP_DATA_DIR), so the avatar it loads never replaces
# the one in your settings; snapshotsLocation is seeded so no folder dialog blocks it.
set -e
D=$(cd "$(dirname "$0")" && pwd)
APP=${1:-"$D/../../build/interface/Darkride.app"}
R=${2:-"${TMPDIR:-/tmp}/spring-check"}
AVATAR=${3:-${SPRING_CHECK_AVATAR:?give the avatar .fst as the third argument or in SPRING_CHECK_AVATAR}}
WORLD="$D/../physics-baseline/world.json"
mkdir -p "$R/config/Tivoli Cloud VR - dev" "$R/data" "$R/snaps"
[ -f "$R/config/Tivoli Cloud VR - dev/Interface.json" ] || echo "{\"snapshotsLocation\": \"$R/snaps\"}" > "$R/config/Tivoli Cloud VR - dev/Interface.json"
BIN="$APP/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
[ -f "$AVATAR" ] || { echo "no avatar at $AVATAR" >&2; exit 1; }
{ echo "var SPRING_CHECK_AVATAR = \"file://$AVATAR\";"; cat "$D/spring-check.js"; } > "$R/spring-check.js"
echo "client: $BIN"
echo "avatar: $AVATAR"
echo "results: $R"
XDG_CONFIG_HOME="$R/config" TIVOLI_APP_DATA_DIR="$R/data" HIFI_METAVERSE_URL=https://vr.darkride.ai \
    "$BIN" --allowMultipleInstances --url "file://$WORLD" --testScript "$R/spring-check.js" --testResultsLocation "$R" > "$R/client.log" 2>&1 || true
grep "SPRING-CHECK\|spring bones" "$R/client.log" || { echo "no summary in $R/client.log" >&2; exit 1; }
