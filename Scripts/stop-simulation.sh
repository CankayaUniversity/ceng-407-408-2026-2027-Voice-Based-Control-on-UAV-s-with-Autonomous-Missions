#!/usr/bin/env bash
set -Eeuo pipefail

SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
GUI_CONTAINER="${GUI_CONTAINER:-swarm-drones-gazebo-gui}"
STOP_TIMEOUT="${STOP_TIMEOUT:-10}"

if ! command -v docker >/dev/null 2>&1; then
    printf 'Required command not found: docker\n' >&2
    exit 1
fi

for container in "${GUI_CONTAINER}" "${SIM_CONTAINER}"; do
    if ! docker inspect "${container}" >/dev/null 2>&1; then
        printf 'Required container not found: %s\n' "${container}" >&2
        exit 1
    fi
done

for container in "${GUI_CONTAINER}" "${SIM_CONTAINER}"; do
    running="$(docker inspect --format '{{.State.Running}}' "${container}")"
    if [[ "${running}" == true ]]; then
        printf 'Stopping %s...\n' "${container}"
        docker stop --time "${STOP_TIMEOUT}" "${container}" >/dev/null
    else
        printf '%s is already stopped.\n' "${container}"
    fi
done

printf 'Gazebo, PX4 SITL, and the swarm are stopped. Workspace files are preserved.\n'
