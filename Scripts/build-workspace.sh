#!/usr/bin/env bash
set -Eeuo pipefail

SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
SIM_UID="${SIM_UID:-1000}"
SIM_GID="${SIM_GID:-1000}"
BUILD_WORKERS="${BUILD_WORKERS:-2}"

if [[ ! "${BUILD_WORKERS}" =~ ^[1-9][0-9]*$ ]]; then
    printf 'BUILD_WORKERS must be a positive integer (got %s).\n' "${BUILD_WORKERS}" >&2
    exit 2
fi

if ! command -v docker >/dev/null 2>&1; then
    printf 'Required command not found: docker\n' >&2
    exit 1
fi

if ! docker inspect "${SIM_CONTAINER}" >/dev/null 2>&1; then
    printf 'Simulation container %s was not found. Create the project containers first.\n' \
        "${SIM_CONTAINER}" >&2
    exit 1
fi

if [[ "$(docker inspect --format '{{.State.Running}}' "${SIM_CONTAINER}")" != true ]]; then
    printf 'Starting build container %s...\n' "${SIM_CONTAINER}"
    docker start "${SIM_CONTAINER}" >/dev/null
fi

if docker exec "${SIM_CONTAINER}" bash -lc 'pgrep -x px4 >/dev/null'; then
    printf 'PX4 is running in %s. Stop the simulation with ./Scripts/stop-simulation.sh, then rerun this build.\n' \
        "${SIM_CONTAINER}" >&2
    exit 1
fi

printf 'Building the ROS 2 workspace in %s with %s workers...\n' \
    "${SIM_CONTAINER}" "${BUILD_WORKERS}"
# ROS setup scripts reference unset optional variables, so keep nounset
# disabled in this child shell while retaining errexit and pipefail.
docker exec \
    --user "${SIM_UID}:${SIM_GID}" \
    --env "BUILD_WORKERS=${BUILD_WORKERS}" \
    "${SIM_CONTAINER}" \
    bash -lc 'set -Ee -o pipefail
        source /opt/ros/jazzy/setup.bash
        cd /workspace
        if [[ ! -d worktrees/simulation/src/uav_comms_bringup \
            || ! -d worktrees/swarm/src/uav_comms_interfaces \
            || ! -d worktrees/agent/src/uav_comms_px4_bridge \
            || ! -f worktrees/agent/px4_msgs/package.xml \
            || ! -f worktrees/agent/px4_ros_com/package.xml \
            || ! -x worktrees/autopilot/autopilot/build/px4_sitl_default/bin/px4 ]]; then
            printf "Expected swarm, simulation, agent and PX4 worktrees are missing under /workspace.\n" >&2
            exit 1
        fi

        # apt updates or a recreated container can change the ROS library
        # versions while generated Makefiles still reference the old
        # absolute paths. Those stale dependencies fail before CMake can
        # reconfigure, so detect them and clear only generated ROS workspace
        # output before rebuilding.
        stale_ros_libraries=()
        while IFS= read -r library; do
            if [[ ! -e "${library}" ]]; then
                stale_ros_libraries+=("${library}")
            fi
        done < <(
            grep -rhoE "/opt/ros/jazzy/lib/[^[:space:];\")]*\\.so(\\.[0-9]+)*" build install 2>/dev/null \
                | sort -u || true
        )
        if ((${#stale_ros_libraries[@]})); then
            printf "Found stale ROS library references in the colcon workspace:\n"
            printf "  %s\n" "${stale_ros_libraries[@]}"
            printf "Removing generated ROS workspace directories and rebuilding.\n"
            rm -rf build install log
        fi

        colcon build --base-paths \
            worktrees/swarm/src worktrees/simulation/src worktrees/agent/src \
            worktrees/agent/px4_msgs worktrees/agent/px4_ros_com \
            --symlink-install --parallel-workers "${BUILD_WORKERS}"
        source /workspace/install/setup.bash
        ros2 pkg prefix uav_comms_bringup >/dev/null
    '

printf 'Workspace build completed. Start the simulation with ./Scripts/restart-simulation.sh\n'
