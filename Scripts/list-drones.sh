#!/usr/bin/env bash
set -Eeuo pipefail

SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
SIM_UID="${SIM_UID:-1000}"
SIM_GID="${SIM_GID:-1000}"

if ! command -v docker >/dev/null 2>&1; then
    printf 'Required command not found: docker\n' >&2
    exit 1
fi

if [[ "$(docker inspect --format '{{.State.Running}}' "${SIM_CONTAINER}" 2>/dev/null || true)" != true ]]; then
    printf 'Simulation container %s is not running. Start it with ../simulation/Scripts/restart-simulation.sh\n' \
        "${SIM_CONTAINER}" >&2
    exit 1
fi

topics="$(docker exec --user "${SIM_UID}:${SIM_GID}" "${SIM_CONTAINER}" \
    bash -lc 'source /opt/ros/jazzy/setup.bash && source /workspace/install/setup.bash && ros2 topic list')"

printf '%s\n' "${topics}" | awk '
    /\/px4_[0-9]+\/odometry$/ {
        topic = $0
        vehicle_id = topic
        sub(/^.*\/px4_/, "", vehicle_id)
        sub(/\/odometry$/, "", vehicle_id)

        namespace = topic
        sub(/\/px4_[0-9]+\/odometry$/, "", namespace)

        printf "Drone %s | namespace: %s | odometry: %s\n", vehicle_id, namespace, topic
        count++
    }
    END {
        if (count == 0) {
            print "No active drone odometry topics found."
        }
    }
'
