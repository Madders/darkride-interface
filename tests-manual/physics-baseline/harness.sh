# harness.sh: sourced by the physics-baseline runners (PHYSICS.md round 9). The pieces every runner needs, written
# once: clients walled off from the user's own settings and identity, the sandbox started and stopped by the pids it
# recorded, the earlier runs' entities taken out of its world, and each client's ktx cache deleted when it exits
# (0.3 to 1.5 GB each; forty of them filled the disk on 10-18). POSIX sh, so the sh runners (run.sh, run-pair.sh)
# and the zsh ones can all source it. The caller sets BIN (the client), R (the results dir), SANDBOX and
# DOMAIN_SANDBOX before sourcing.
#
#   wall <name>                      $R/<name>-home with config, data and snaps, Interface.json seeded with the
#                                    snapshots folder (without it a folder dialog blocks the client)
#   client <name> <args...>          the client walled off in $R/<name>-home, --allowMultipleInstances (a second
#                                    client would otherwise hand its --url to one already running and exit) and
#                                    --tokens "{}" (no sign-in), stdout and stderr to $R/<name>.log; the ktx cache
#                                    is deleted when it exits (kept with KEEP_KTX=1, for a warm run of the same
#                                    client next). Environment set by the caller (TIVOLI_PACKET_LOSS,
#                                    TIVOLI_PACKET_DELAY_MS) passes through.
#   sandbox_stop                     stops this sandbox's servers by the pids in $SANDBOX/*.pid, each checked to be a
#                                    domain-server or assignment-client of ours, and deletes the pid files
#   sandbox_clean_world <name>...    takes the entities whose name starts with (or is) one of the names out of the
#                                    sandbox's world file; an earlier run's entities (lifetime 600 s) would otherwise
#                                    still be in the world beside this run's
#   sandbox_start <build> <port>     turns the physics server on in the sandbox's config (off by default), starts the
#                                    sandbox from the build with domain-sandbox.sh and waits for the physics server
#                                    (the sandbox's monitor adds the seventh service at its spare check, up to 30 s)
#   sandbox_copy_logs                copies the sandbox's assignment-client and domain-server logs beside the results
#                                    (a restart overwrites them)
#   wait_for_client <name> [s]       waits until the client's log says its physics is on (its loading screen has lifted:
#                                    the world is in), up to s seconds (180): the second client of a pair must not start
#                                    before the first has the world, or it misses the cabin (a cold cache under 5 % loss
#                                    took 90 s to load the sandbox's world on 10-19, 20 s without loss)

SANDBOX=${SANDBOX:-$HOME/tivoli-sandboxes/physics}
DOMAIN_SANDBOX=${DOMAIN_SANDBOX:-$HOME/git/tivolicloud/domain-sandbox.sh}   # darkride-ops' sandbox starter
export HIFI_METAVERSE_URL=${HIFI_METAVERSE_URL:-https://vr.darkride.ai}

wall() {
    mkdir -p "$R/$1-home/config/Tivoli Cloud VR - dev" "$R/$1-home/data" "$R/$1-home/snaps"
    [ -f "$R/$1-home/config/Tivoli Cloud VR - dev/Interface.json" ] ||
        echo "{\"snapshotsLocation\": \"$R/$1-home/snaps\"}" > "$R/$1-home/config/Tivoli Cloud VR - dev/Interface.json"
}

client() {
    _name=$1; shift
    wall "$_name"
    XDG_CONFIG_HOME="$R/$_name-home/config" TIVOLI_APP_DATA_DIR="$R/$_name-home/data" \
        "$BIN" --allowMultipleInstances --tokens "{}" "$@" > "$R/$_name.log" 2>&1
    _status=$?
    [ -n "$KEEP_KTX" ] || rm -rf "$R/$_name-home/data/ktx_cache"
    return $_status
}

sandbox_stop() {
    for _pidfile in $(find "$SANDBOX" -maxdepth 1 -name '*.pid' 2>/dev/null); do
        _pid=$(cat "$_pidfile" 2>/dev/null)
        if [ -n "$_pid" ] && ps -o command= -p "$_pid" 2>/dev/null | grep -q "domain-server\|assignment-client"; then
            kill "$_pid" 2>/dev/null || true
        fi
        rm -f "$_pidfile"
    done
    sleep 3
}

sandbox_clean_world() {
    # both the entity server's file and the domain-server's copy (appdata/entities), which replaces the entity
    # server's at start when its data version is newer (see run-loading.sh)
    python3 - "$SANDBOX/entities/models.json.gz" "$SANDBOX/appdata/entities/models.json.gz" "$@" <<'PY'
import gzip, json, os, sys
paths, names = sys.argv[1:3], sys.argv[3:]
for path in paths:
    if not os.path.exists(path):
        continue
    try:
        world = json.load(gzip.open(path))
    except Exception as e:
        print("no world file to clean at", path, ":", e); continue
    before = len(world.get("Entities", []))
    world["Entities"] = [e for e in world.get("Entities", [])
                         if not any(str(e.get("name", "")).startswith(n) for n in names)]
    with gzip.open(path, "wt") as f:
        json.dump(world, f)
    print("world cleaned of earlier runs' test entities:", before - len(world["Entities"]), "removed,", len(world["Entities"]), "kept, in", path)
PY
}

sandbox_start() {
    _build=$1; _port=$2
    python3 -c 'import json, os, sys; p = sys.argv[1]; c = json.load(open(p)) if os.path.exists(p) else {}; c.setdefault("physics_server", {})["enabled"] = True; json.dump(c, open(p, "w"), indent=2)' "$SANDBOX/domain-config.json"
    "$DOMAIN_SANDBOX" physics "$_build" "$_port"
    _i=0
    while [ "$_i" -lt 90 ]; do
        _i=$((_i + 1))
        curl -s "http://localhost:$((_port - 2))/nodes.json" 2>/dev/null | grep -q '"physics-server"' && break
        sleep 1
    done
    echo "physics-server in the domain after ${_i}s"
}

wait_for_client() {
    _name=$1; _limit=${2:-180}; _i=0
    while [ "$_i" -lt "$_limit" ]; do
        grep -q "Physics enabled" "$R/$_name.log" 2>/dev/null && break
        _i=$((_i + 1)); sleep 1
    done
    echo "$_name has the world after ${_i}s"
}

sandbox_copy_logs() {
    cp "$SANDBOX/logs/assignment-client.log" "$R/sandbox-assignment-client.log" 2>/dev/null || true
    cp "$SANDBOX/logs/domain-server.log" "$R/sandbox-domain-server.log" 2>/dev/null || true
}
