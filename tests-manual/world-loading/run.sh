#!/bin/zsh
# One arrival in a test world, in either loading mode, on this session's own sandbox (the client skips the sign-in
# screen with --tokens "{}", so the arrival is timed from the connection):
#   tests-manual/world-loading/run.sh <build dir> <results dir> <whole|progressive> [world dir] [skip after s] [udp port]
# LOADING_RADIUS=<metres> sets the domain's loading.radius (default 100; 0 waits for every model in the world), and
# LOADING_TIME_LIMIT=<seconds> its loading.time_limit (default 60). HOP=1 arrives by a hop instead, as Explore makes
# one (hop.js): the client starts in a file world, says on "Darkride-Hop" that a hop has started, and goes to the
# domain 3 s later (HOP_DELAY=<ms> to change it: Explore goes at once); the loading screen must cover the whole way.
# RENDER_METHOD=0|1 (with HOP=1): draw deferred (0) or forward (1) from the landing on; every graphics preset sets
# forward at start-up, so the hop script sets it once it has landed.
# HOP_PRESS=<button>@<s> (with HOP=1): press the loading screen's "cancel" or "showNow" that long after the hop.
# KEEP_CACHE=<dir>: keep the client's resource caches there between runs (the second run arrives warm).
# HOP_CHECK_SCRIPTS=1 (with HOP=1): 15 s after landing, count Tivoli Town's plants its hideByDistance.js has hidden
# (whether the world's entity scripts run).
# HOP_SCRIPT=<file in this folder> (with HOP=1): another hop test instead of hop.js, e.g. cancel-twice.js or leave.js;
# HOP_VARS="var X = 1;" sets its options.
# HOP_AGAIN=<s> (with HOP=1): that long after landing, hop again to the same world (it must lift after its minimum).
# CLIENT_ARGS="<args>": more arguments for the client, e.g. --scripts <repo>/scripts
# to run the system scripts from the source tree (a script change tested without a build).
# HOP_TO=<address> (with HOP=1): hop there instead, and leave the sandbox alone (not started, copied or stopped): a
# world that doesn't answer (hifi://localhost:<a port nothing listens on>) or a place that doesn't exist, whose
# screen must lift as the viewer gives up (its connection failure window, or the place not found), not at its 90 s.
# The world dir has the snapshot layout of instances/tivoli-instance.sh (models.json.gz, config.json with the spawn
# point, assets/map.json and assets/files/<sha256>); the default is the Tivoli Town copy in ~/tivoli-test-worlds.
# The sandbox ("loading", ~/tivoli-sandboxes/loading) gets a fresh copy of the world and the domain's loading
# settings each run, starts, takes one walled-off client running arrival.js, and is stopped by its pids afterwards.
# The client is walled off (its own settings and app data), and its resource cache is fresh each run (a cold arrival):
# the caches live in TIVOLI_APP_DATA_DIR (data/cache, data/ktx_cache; ~/Library/Caches is not touched, checked 10-18),
# which points into the run and is deleted after it.
set -e
D=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:?usage: run.sh <build dir> <results dir> <whole|progressive> [world dir] [skip after s] [udp port]}
R=${2:?results dir}
MODE=${3:?whole or progressive}
WORLD=${4:-$HOME/tivoli-test-worlds/tivolitown-v2}
SKIP=${5:--1}
PORT=${6:-42102}
SANDBOX=$HOME/tivoli-sandboxes/loading
# the client is Darkride.app since main 133685ae0f (the app's rename), interface.app before
APP="$BUILD/interface/Darkride.app"
[ -d "$APP" ] || APP="$BUILD/interface/interface.app"
BIN="$APP/Contents/MacOS/$(basename "$APP" .app)"
[ -x "$BIN" ] || { echo "no client at $BIN" >&2; exit 1; }
[ -f "$WORLD/models.json.gz" ] || { echo "no world at $WORLD" >&2; exit 1; }
case "$MODE" in whole|progressive) ;; *) echo "mode is whole or progressive" >&2; exit 1;; esac

if [ -z "$HOP_TO" ]; then
# the world, fresh: the entity server persists into its copy
mkdir -p "$SANDBOX/entities" "$SANDBOX/assets"
for pidfile in "$SANDBOX"/*.pid(N); do
    pid=$(cat "$pidfile"); case "$(ps -o command= -p $pid 2>/dev/null)" in *"$BUILD"*) kill $pid;; esac; rm -f "$pidfile"
done
cp "$WORLD/models.json.gz" "$SANDBOX/entities/models.json.gz"
# and the domain-server's own copy, which it hands its entity server when the ids differ (a world other than the last
# one run would otherwise be replaced by that one)
mkdir -p "$SANDBOX/appdata/entities"
cp "$WORLD/models.json.gz" "$SANDBOX/appdata/entities/models.json.gz"
rm -rf "$SANDBOX/assets"; mkdir -p "$SANDBOX/assets"
[ -d "$WORLD/assets" ] && cp -R "$WORLD/assets/." "$SANDBOX/assets/"
# the domain's settings: the loading mode and the world's spawn point
python3 - "$SANDBOX/domain-config.json" "$WORLD/config.json" "$MODE" <<'PY'
import json, os, sys
path, world_config, mode = sys.argv[1:4]
config = json.load(open(path)) if os.path.exists(path) else {}
world = json.load(open(world_config)) if os.path.exists(world_config) else {}
config["loading"] = {"whole_world": mode == "whole", "time_limit": int(os.environ.get("LOADING_TIME_LIMIT") or 60)}
if os.environ.get("LOADING_RADIUS"):
    config["loading"]["radius"] = int(os.environ["LOADING_RADIUS"])  # metres; 0 waits for the whole world
if world.get("paths"):
    config["paths"] = world["paths"]
json.dump(config, open(path, "w"), indent=2)
PY
"${DOMAIN_SANDBOX:-$HOME/git/tivolicloud/domain-sandbox.sh}" loading "$BUILD" "$PORT"   # darkride-ops' sandbox starter
for i in {1..60}; do curl -s "localhost:$((PORT - 2))/nodes.json" | grep -q entity-server && break; sleep 1; done
fi

# one walled-off client, with a fresh cache
RUN="$R/$MODE-$(date +%H%M%S)"
mkdir -p "$RUN/config/Tivoli Cloud VR - dev" "$RUN/data" "$RUN/snaps" "$RUN/cache"
# KEEP_CACHE=<dir>: the client's app data (its resource caches) lives there instead, and is kept: a second run with the
# same dir arrives warm, as someone coming back to a world they have been to
DATA="$RUN/data"
[ -n "$KEEP_CACHE" ] && { mkdir -p "$KEEP_CACHE"; DATA="$KEEP_CACHE"; }
# CLIENT_WRAPPER="<command and args>" runs the client under it, e.g. lldb --batch -o run -k "thread backtrace all" --
# CARD=1: the loading screen's headset card on the desktop too, to see it in the snapshots
CARD_SETTING=""
[ -n "$CARD" ] && CARD_SETTING=", \"worldLoading/cardOnDesktop\": true"
echo "{\"snapshotsLocation\": \"$RUN/snaps\"$CARD_SETTING}" > "$RUN/config/Tivoli Cloud VR - dev/Interface.json"
# BASE_SETTINGS=<Interface.json>: start the walled-off client from a copy of those settings (someone's render options,
# to reproduce what they see); the copy is the wall's, the file itself is only read
if [ -n "$BASE_SETTINGS" ]; then
    python3 - "$BASE_SETTINGS" "$RUN/config/Tivoli Cloud VR - dev/Interface.json" <<'PY'
import json, sys
base, wall = sys.argv[1:3]
settings = json.load(open(base))
settings.pop("sessionRunTime", None)
settings.update(json.load(open(wall)))
json.dump(settings, open(wall, "w"), indent=2)
PY
fi
{ echo "var ARRIVAL_SKIP_AFTER = $SKIP;"; cat "$D/arrival.js"; } > "$RUN/arrival.js"
URL="hifi://localhost:$PORT"
TEST_SCRIPT="$RUN/arrival.js"
if [ -n "$HOP" ]; then
    { echo "var HOP_URL = \"${HOP_TO:-hifi://localhost:$PORT}\";"; [ -n "$HOP_DELAY" ] && echo "var HOP_DELAY = $HOP_DELAY;"; [ -n "$PICTURES" ] && echo "var HOP_PICTURES = \"$RUN\";"; [ -n "$RENDER_METHOD" ] && echo "var HOP_RENDER_METHOD = $RENDER_METHOD;"; [ -n "$HOP_PRESS" ] && echo "var HOP_PRESS = \"$HOP_PRESS\";"; [ -n "$HOP_AGAIN" ] && echo "var HOP_AGAIN = $HOP_AGAIN;"; [ -n "$HOP_CHECK_SCRIPTS" ] && echo "var HOP_CHECK_SCRIPTS = 1;"; [ -n "$HOP_VARS" ] && echo "$HOP_VARS"; cat "$D/${HOP_SCRIPT:-hop.js}"; } > "$RUN/hop.js"
    URL="file://$D/../physics-baseline/world.json"
    TEST_SCRIPT="$RUN/hop.js"
fi
echo "client: $BIN  mode: $MODE  world: $WORLD  results: $RUN"
XDG_CONFIG_HOME="$RUN/config" XDG_CACHE_HOME="$RUN/cache" TIVOLI_APP_DATA_DIR="$DATA" HIFI_METAVERSE_URL=https://vr.darkride.ai \
    ${=CLIENT_WRAPPER} "$BIN" --allowMultipleInstances --url "$URL" --tokens "{}" --testScript "$TEST_SCRIPT" \
    --testResultsLocation "$RUN" ${=CLIENT_ARGS} > "$RUN/client.log" 2>&1 || true

# the sandbox off again, by its pids
if [ -z "$HOP_TO" ]; then
for pidfile in "$SANDBOX"/*.pid(N); do
    pid=$(cat "$pidfile"); case "$(ps -o command= -p $pid 2>/dev/null)" in *"$BUILD"*) kill $pid;; esac; rm -f "$pidfile"
done
fi
# the client's caches (2 GB a run for Tivoli Town, the textures in data/ktx_cache) go; the log and snapshots stay
rm -rf "$RUN/data" "$RUN/cache"
grep "WORLD-LOADING\|World loading" "$RUN/client.log" || { echo "no summary in $RUN/client.log" >&2; exit 1; }
