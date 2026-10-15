#!/bin/bash
# Runs a whole world in one container: the domain server and one assignment client per assignment type,
# each on a fixed UDP port so that the ports can be published (docker run -p) or opened in a firewall.
# If any of them exits, the rest are stopped and the container exits, for the restart policy to act on.
#
# Environment (defaults in brackets):
#   TIVOLI_APP_DATA_DIR                               where settings, the world and assets are kept [/data]
#   HIFI_DOMAIN_SERVER_HTTP_PORT / _HTTPS_PORT        domain server web interface, TCP [40100 / 40101]
#   HIFI_ASSIGNMENT_CLIENT_<TYPE>_PORT                assignment client UDP ports [48000-48006]
#   DOMAIN_SERVER_ARGS                                extra domain-server arguments, e.g. "-i ice.example.org:7337"
#
# The domain server's own UDP port (40102) is its "metaverse.local_port" setting, changed on its settings page.

cd "$(dirname "$0")"

port() {
    local variable="HIFI_ASSIGNMENT_CLIENT_$1_PORT"
    echo "${!variable:-$2}"
}

pids=()
stop() {
    kill "${pids[@]}" 2>/dev/null
    wait
}
trap 'stop; exit 0' TERM INT

./domain-server $DOMAIN_SERVER_ARGS &
pids+=($!)

# type, name of the port variable, default port
for assignment in "0 AUDIO_MIXER 48000" "1 AVATAR_MIXER 48001" "3 ASSET_SERVER 48003" \
    "4 MESSAGES_MIXER 48004" "5 ENTITY_SCRIPT_SERVER 48005" "6 ENTITIES_SERVER 48006"; do
    set -- $assignment
    ./assignment-client -t "$1" -a localhost -p "$(port "$2" "$3")" &
    pids+=($!)
done

# scripted agents (type 2) take any free port; the monitor forks up to --max of them
./assignment-client -t 2 -a localhost --max "${AGENT_MAX:-100}" &
pids+=($!)

wait -n
status=$?
echo "A server process exited with status $status; stopping the rest" >&2
stop
exit "$status"
