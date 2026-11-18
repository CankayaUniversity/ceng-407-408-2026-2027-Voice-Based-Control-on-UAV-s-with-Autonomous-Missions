#!/usr/bin/env bash
set -Eeuo pipefail

SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
SIM_UID="${SIM_UID:-1000}"
SIM_GID="${SIM_GID:-1000}"
GZ_WORLD="${GZ_WORLD:-lawn}"

usage() {
    printf 'Usage: %s VEHICLE_ID X,Y[,Z[,ROLL,PITCH,YAW]]\n' "$0" >&2
    printf 'Example: %s 3 5,0\n' "$0" >&2
}

if [[ $# -ne 2 || ! "$1" =~ ^[0-9]+$ ]]; then
    usage
    exit 2
fi

vehicle_id="$1"
model_pose="$2"
IFS=, read -r -a pose_parts <<< "${model_pose}"
if [[ ! "${#pose_parts[@]}" =~ ^(2|3|6)$ ]]; then
    printf 'Pose must contain 2, 3, or 6 comma-separated numbers.\n' >&2
    usage
    exit 2
fi
for coordinate in "${pose_parts[@]}"; do
    if [[ ! "${coordinate}" =~ ^-?([0-9]+([.][0-9]*)?|[.][0-9]+)$ ]]; then
        printf 'Invalid pose coordinate: %s\n' "${coordinate}" >&2
        exit 2
    fi
done

if ! command -v docker >/dev/null 2>&1; then
    printf 'Required command not found: docker\n' >&2
    exit 1
fi
if [[ "$(docker inspect --format '{{.State.Running}}' "${SIM_CONTAINER}" 2>/dev/null || true)" != true ]]; then
    printf 'Simulation container %s is not running. Start it with ./Scripts/restart-simulation.sh\n' "${SIM_CONTAINER}" >&2
    exit 1
fi

nodes="$(docker exec --user "${SIM_UID}:${SIM_GID}" "${SIM_CONTAINER}" \
    bash -lc 'source /opt/ros/jazzy/setup.bash && source /workspace/install/setup.bash && ros2 node list')"
bridge_node="/uav_comms/px4_${vehicle_id}/px4_bridge"
if grep -Fxq "${bridge_node}" <<< "${nodes}"; then
    printf 'Vehicle ID %s is already running (%s). Choose a new ID.\n' "${vehicle_id}" "${bridge_node}" >&2
    exit 1
fi

printf 'Spawning vehicle %s at model pose %s. Keep this terminal open; Ctrl-C stops this added vehicle.\n' \
    "${vehicle_id}" "${model_pose}"
docker exec -it \
    --user "${SIM_UID}:${SIM_GID}" \
    -e PX4_DIR=/workspace/worktrees/autopilot/autopilot \
    -e VEHICLE_ID="${vehicle_id}" \
    -e MODEL_POSE="${model_pose}" \
    -e GZ_WORLD="${GZ_WORLD}" \
    "${SIM_CONTAINER}" \
    bash -lc 'cd /workspace \
        && source /opt/ros/jazzy/setup.bash \
        && source /workspace/install/setup.bash \
        && exec ros2 launch uav_comms_bringup spawn_drone.launch.py \
            vehicle_id:="${VEHICLE_ID}" \
            model_pose:="${MODEL_POSE}" \
            px4_sys_autostart:=4001 \
            px4_sim_model:=gz_x500 \
            gz_world:="${GZ_WORLD}" \
            gz_partition:=default \
            gz_ip:=127.0.0.1 \
            viz_enabled:=false \
            swarm_agent_enabled:=true'
