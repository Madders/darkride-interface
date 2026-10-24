#!/bin/zsh
# A walled-off client in a serverless world, running dump-draws.js, then diff-dump.py on its log:
#   tests-manual/instancing/run-dump.sh <build dir> <results dir> <world.json> [heading]
# The world file's layout is ~/tivoli-test-perf's serverless copies (make-serverless-world.py).
set -e
D=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:?usage: run-dump.sh <build dir> <results dir> <world.json> [heading]}
R=${2:?results dir}
WORLD=${3:?world.json}
HEADING=${4:-90}
BIN="$BUILD/interface/Darkride.app/Contents/MacOS/Darkride"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
RUN="$R/dump-$(date +%H%M%S)"
mkdir -p "$RUN/config/Tivoli Cloud VR - dev" "$RUN/data" "$RUN/cache"
{ echo "var DUMP_HEADING = $HEADING;"; cat "$D/dump-draws.js"; } > "$RUN/dump-draws.js"
XDG_CONFIG_HOME="$RUN/config" XDG_CACHE_HOME="$RUN/cache" TIVOLI_APP_DATA_DIR="$RUN/data" HIFI_METAVERSE_URL=https://vr.darkride.ai \
    "$BIN" --allowMultipleInstances --url "file://$WORLD" --tokens "{}" --testScript "$RUN/dump-draws.js" \
    --testResultsLocation "$RUN" > "$RUN/client.log" 2>&1 || true
rm -rf "$RUN/data" "$RUN/cache"
grep "INSTANCING-TEST" "$RUN/client.log" | sed -E 's/^\[[^]]*\] //' || true
python3 -I "$D/diff-dump.py" "$RUN/client.log"
echo "log: $RUN/client.log"
