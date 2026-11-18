#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
GUI_CONTAINER="${GUI_CONTAINER:-swarm-drones-gazebo-gui}"
SIM_UID="${SIM_UID:-1000}"
SIM_GID="${SIM_GID:-1000}"
GZ_WORLD="${GZ_WORLD:-lawn}"
DISPLAY="${DISPLAY:-:0}"
HOST_XUSER="$(id -un)"
XHOST_GRANTED=0

cleanup() {
    if [[ "${XHOST_GRANTED}" == 1 ]]; then
        DISPLAY="${DISPLAY}" xhost -SI:localuser:"${HOST_XUSER}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

for command in docker xhost; do
    if ! command -v "${command}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command}" >&2
        exit 1
    fi
done

for container in "${SIM_CONTAINER}" "${GUI_CONTAINER}"; do
    if ! docker inspect "${container}" >/dev/null 2>&1; then
        printf 'Required container not found: %s\n' "${container}" >&2
        exit 1
    fi
    workspace_mount="$(docker inspect --format '{{range .Mounts}}{{if eq .Destination "/workspace"}}{{.Source}}{{end}}{{end}}' "${container}")"
    if [[ "${workspace_mount}" != "${PROJECT_ROOT}" ]]; then
        printf 'Container %s mounts %s at /workspace, but this project is %s.\n' \
            "${container}" "${workspace_mount:-<nothing>}" "${PROJECT_ROOT}" >&2
        printf 'Run ./Scripts/setup-containers.sh from worktrees/simulation with new SIM_CONTAINER and GUI_CONTAINER names to keep these containers, or use --replace if they can be removed.\n' >&2
        exit 1
    fi
done

printf 'Restarting simulation container %s...\n' "${SIM_CONTAINER}"
docker restart "${SIM_CONTAINER}" >/dev/null

docker exec -d \
    --user "${SIM_UID}:${SIM_GID}" \
    -e PX4_DIR=/workspace/worktrees/autopilot/autopilot \
    -e GZ_WORLD="${GZ_WORLD}" \
    "${SIM_CONTAINER}" \
    bash -lc 'cd /workspace \
        && source /opt/ros/jazzy/setup.bash \
        && source /workspace/install/setup.bash \
        && ros2 launch uav_comms_bringup swarm_sim.launch.py \
            gz_world:="${GZ_WORLD}" headless:=true viz_enabled:=false mission_enabled:=true swarm_agent_enabled:=true \
            > /workspace/simulation.log 2>&1'

printf 'Waiting for the swarm to become ready...\n'
ready=0
for ((attempt = 0; attempt < 60; attempt++)); do
    if docker exec --user "${SIM_UID}:${SIM_GID}" "${SIM_CONTAINER}" \
        bash -lc 'source /opt/ros/jazzy/setup.bash \
            && source /workspace/install/setup.bash \
            && ros2 topic list | grep -qx /uav_comms/swarm/members' \
        >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 2
done

if [[ "${ready}" != 1 ]]; then
    printf 'Swarm did not become ready within 120 seconds. Recent launch log:\n' >&2
    docker exec "${SIM_CONTAINER}" tail -n 60 /workspace/simulation.log >&2 || true
    exit 1
fi

printf 'Reconnecting NVIDIA Gazebo GUI on %s...\n' "${DISPLAY}"
DISPLAY="${DISPLAY}" xhost +SI:localuser:"${HOST_XUSER}" >/dev/null
XHOST_GRANTED=1
DISPLAY="${DISPLAY}" docker restart "${GUI_CONTAINER}" >/dev/null
docker exec -d \
    --user "${SIM_UID}:${SIM_GID}" \
    --env PX4_DIR=/workspace/worktrees/autopilot/autopilot \
    --env "DISPLAY=${DISPLAY}" \
    --env GZ_PARTITION=default \
    --env GZ_IP=127.0.0.1 \
    "${GUI_CONTAINER}" \
    bash -lc 'source /opt/ros/jazzy/setup.bash \
        && source "${PX4_DIR}/build/px4_sitl_default/rootfs/gz_env.sh" \
        && export GZ_PARTITION=default GZ_IP=127.0.0.1 \
        && exec gz sim -g > /workspace/gazebo-gui.log 2>&1'

gui_started=0
for ((attempt = 0; attempt < 15; attempt++)); do
    if docker exec --user "${SIM_UID}:${SIM_GID}" "${GUI_CONTAINER}" \
        pgrep -f '^gz sim -g$' >/dev/null 2>&1; then
        gui_started=1
        break
    fi
    sleep 1
done

if [[ "${gui_started}" != 1 ]]; then
    printf 'Gazebo GUI did not stay running. Recent GUI log:\n' >&2
    docker exec "${GUI_CONTAINER}" tail -n 80 /workspace/gazebo-gui.log >&2 || true
    exit 1
fi

printf 'Simulation and GUI restarted.\n'
printf 'Launch log: /workspace/simulation.log inside %s\n' "${SIM_CONTAINER}"
printf 'Follow it with: docker exec -it %s tail -f /workspace/simulation.log\n' "${SIM_CONTAINER}"
