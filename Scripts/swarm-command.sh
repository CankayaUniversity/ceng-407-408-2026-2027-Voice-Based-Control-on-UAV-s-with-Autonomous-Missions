#!/usr/bin/env bash
# Shared implementation for the public swarm command wrappers.
set -Eeuo pipefail

command_name="$1"
shift
SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
SIM_UID="${SIM_UID:-1000}"
SIM_GID="${SIM_GID:-1000}"

case "${command_name}" in
    create-swarm) syntax='id:<swarm_id>' ;;
    conscript-drone) syntax='id:<drone_id> swarm_id:<swarm_id>' ;;
    go-mission) syntax='x:<x> y:<y> z:<z> swarm_id:<swarm_id>' ;;
    *) exit 2 ;;
esac
if [[ $# == 0 || "${1:-}" == --help || "${1:-}" == -h ]]; then
    printf 'Usage: ./Scripts/%s.sh %s\n' "${command_name}" "${syntax}"
    if [[ $# == 0 ]]; then exit 2; fi
    exit 0
fi
if ! command -v docker >/dev/null 2>&1; then
    printf 'Required command not found: docker\n' >&2
    exit 1
fi
if [[ "$(docker inspect --format '{{.State.Running}}' "${SIM_CONTAINER}" 2>/dev/null || true)" != true ]]; then
    printf 'Simulation container %s is not running. Start it with ../simulation/Scripts/restart-simulation.sh\n' "${SIM_CONTAINER}" >&2
    exit 1
fi
# Pass arguments positionally; never interpolate user input into shell code.
exec docker exec --user "${SIM_UID}:${SIM_GID}" "${SIM_CONTAINER}" \
    bash -lc 'source /opt/ros/jazzy/setup.bash \
        && source /workspace/install/setup.bash \
        && exec python3 -m uav_comms_mission_planner.swarm_cli "$@"' \
    bash "${command_name}" "$@"
