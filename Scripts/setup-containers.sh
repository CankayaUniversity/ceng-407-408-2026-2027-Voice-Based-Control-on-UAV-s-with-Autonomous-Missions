#!/usr/bin/env bash
set -Eeuo pipefail

# Mount the shared parent so the three subsystem worktrees are visible to colcon.
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
PX4_DIR="${PROJECT_ROOT}/worktrees/autopilot/autopilot"
SIM_CONTAINER="${SIM_CONTAINER:-swarm-drones-sitl}"
GUI_CONTAINER="${GUI_CONTAINER:-swarm-drones-gazebo-gui}"
ROS_IMAGE="${ROS_IMAGE:-osrf/ros:jazzy-desktop}"
MICRO_XRCE_AGENT_VERSION="${MICRO_XRCE_AGENT_VERSION:-v2.4.3}"
SIM_UID="${SIM_UID:-$(id -u)}"
SIM_GID="${SIM_GID:-$(id -g)}"
DISPLAY="${DISPLAY:-:0}"
PX4_BUILD_WORKERS="${PX4_BUILD_WORKERS:-2}"
BUILD_WORKERS="${BUILD_WORKERS:-2}"
ENABLE_GPU="${ENABLE_GPU:-auto}"

usage() {
    cat <<EOF
Usage: $(basename "$0") [--replace]

Pulls the ROS 2 Jazzy image, creates or reuses the project SITL and Gazebo GUI
containers, installs ROS/Gazebo/PX4 dependencies, builds PX4 SITL and the ROS
workspace, and installs the matching Micro XRCE-DDS Agent.

Environment overrides:
  ROS_IMAGE, SIM_CONTAINER, GUI_CONTAINER
  SIM_UID, SIM_GID, DISPLAY, PX4_BUILD_WORKERS, BUILD_WORKERS
  MICRO_XRCE_AGENT_VERSION (default: v2.4.3 for ROS 2 Jazzy)
  ENABLE_GPU=auto|1|0 (auto enables NVIDIA when Docker supports it)
EOF
}

replace_containers=0
while (($#)); do
    case "$1" in
        --replace)
            replace_containers=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

for command in docker git xhost; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "$command" >&2
        exit 1
    fi
done

if [[ ! -d /tmp/.X11-unix ]]; then
    printf 'The host X11 socket directory /tmp/.X11-unix is missing. Start an X11/XWayland session before setting up Gazebo GUI.\n' >&2
    exit 1
fi

if [[ ! "$SIM_UID" =~ ^[0-9]+$ || ! "$SIM_GID" =~ ^[0-9]+$ ]]; then
    printf 'SIM_UID and SIM_GID must be numeric.\n' >&2
    exit 2
fi
for workers in "$PX4_BUILD_WORKERS" "$BUILD_WORKERS"; do
    if [[ ! "$workers" =~ ^[1-9][0-9]*$ ]]; then
        printf 'Build worker counts must be positive integers.\n' >&2
        exit 2
    fi
done

if ! docker info >/dev/null 2>&1; then
    printf 'Docker is not available. Start the Docker daemon and check your Docker permissions.\n' >&2
    exit 1
fi

if [[ ! -f "$PX4_DIR/Makefile" ]]; then
    printf 'PX4 firmware not found at %s; initialize autopilot/master first.\n' "$PX4_DIR" >&2
    exit 1
fi
printf 'Initializing PX4 submodules...\n'
git -C "$PROJECT_ROOT/worktrees/autopilot" submodule update --init --recursive

printf 'Initializing compatible ROS companion submodules...\n'
git -C "$PROJECT_ROOT/worktrees/agent" submodule update --init -- px4_msgs px4_ros_com

printf 'Pulling %s...\n' "$ROS_IMAGE"
docker pull "$ROS_IMAGE"

gpu_args=()
case "$ENABLE_GPU" in
    1|true|yes)
        gpu_args=(--gpus all -e NVIDIA_DRIVER_CAPABILITIES=all)
        ;;
    0|false|no)
        ;;
    auto)
        if command -v nvidia-smi >/dev/null 2>&1 \
            && docker info --format '{{json .Runtimes}}' 2>/dev/null | grep -q 'nvidia'; then
            gpu_args=(--gpus all -e NVIDIA_DRIVER_CAPABILITIES=all)
            printf 'NVIDIA container runtime detected; enabling GPU access.\n'
        else
            printf 'NVIDIA container runtime not detected; containers will use software rendering.\n'
        fi
        ;;
    *)
        printf 'ENABLE_GPU must be auto, 1, or 0 (got %s).\n' "$ENABLE_GPU" >&2
        exit 2
        ;;
    esac

gui_security_args=()
if docker info --format '{{json .SecurityOptions}}' 2>/dev/null | grep -q 'name=selinux'; then
    # The host X server socket is denied to the confined container domain even
    # when the socket is mounted and the X access list permits the host user.
    # Disable SELinux labeling for the GUI container only; keep the SITL
    # container under the daemon's normal SELinux confinement.
    gui_security_args=(--security-opt label=disable)
    printf 'SELinux detected; allowing the GUI container to connect to the host X11 socket.\n'
fi

ensure_container() {
    local name="$1"
    local existing_mount
    local -a container_security_args=()

    if [[ "$name" == "$GUI_CONTAINER" ]]; then
        container_security_args=("${gui_security_args[@]}")
    fi

    if docker inspect "$name" >/dev/null 2>&1; then
        if [[ "$replace_containers" == 1 ]]; then
            printf 'Removing existing container %s; the mounted workspace will be kept.\n' "$name"
            docker rm -f "$name" >/dev/null
        fi
    fi

    if docker inspect "$name" >/dev/null 2>&1; then
        existing_mount="$(docker inspect --format '{{range .Mounts}}{{if eq .Destination "/workspace"}}{{.Source}}{{end}}{{end}}' "$name")"
        if [[ "$existing_mount" != "$PROJECT_ROOT" ]]; then
            printf 'Container %s already exists with a different /workspace mount.\n' "$name" >&2
            printf 'Use --replace to remove that container and recreate it for %s.\n' "$PROJECT_ROOT" >&2
            exit 1
        fi
        network_mode="$(docker inspect --format '{{.HostConfig.NetworkMode}}' "$name")"
        if [[ "$network_mode" != host ]]; then
            printf 'Container %s is using Docker network mode %s; this setup requires host networking for ROS 2 and Gazebo discovery.\n' \
                "$name" "$network_mode" >&2
            printf 'Use --replace to recreate it.\n' >&2
            exit 1
        fi
    fi

    if ! docker inspect "$name" >/dev/null 2>&1; then
        printf 'Creating container %s...\n' "$name"
        # The workspace is shared by both containers. The :z suffix applies
        # a shared SELinux label where SELinux is enabled and is ignored on
        # systems without SELinux.
        docker create \
            --name "$name" \
            --hostname "$name" \
            --label com.uav_comms.swarm.environment=px4-sitl \
            --network host \
            --restart unless-stopped \
            --user "${SIM_UID}:${SIM_GID}" \
            --workdir /workspace \
            --env HOME=/tmp \
            --env DISPLAY="$DISPLAY" \
            --env ROS_DOMAIN_ID=0 \
            --env GZ_PARTITION=default \
            --env GZ_IP=127.0.0.1 \
            --volume "${PROJECT_ROOT}:/workspace:z" \
            --volume /tmp/.X11-unix:/tmp/.X11-unix:z \
            "${container_security_args[@]}" \
            "${gpu_args[@]}" \
            --entrypoint /bin/bash \
            "$ROS_IMAGE" -lc 'sleep infinity' >/dev/null
    fi

    if [[ "$(docker inspect --format '{{.State.Running}}' "$name")" != true ]]; then
        docker start "$name" >/dev/null
    fi

    # Git worktree .git files store absolute host paths. Map the checkout's
    # host path to the shared /workspace mount inside this container so PX4
    # can read its Git metadata when CMake generates build/version headers.
    docker exec --user 0:0 --env "HOST_ROOT=${PROJECT_ROOT}" "$name" bash -lc '
        set -e
        if [[ ! -e "$HOST_ROOT" ]]; then
            mkdir -p "$(dirname "$HOST_ROOT")"
            ln -s /workspace "$HOST_ROOT"
        elif [[ "$(readlink -f "$HOST_ROOT")" != /workspace ]]; then
            printf "Container checkout path %s does not point to /workspace.\n" "$HOST_ROOT" >&2
            exit 1
        fi
    '

    if ! docker exec "$name" test -r /opt/ros/jazzy/setup.bash; then
        printf 'Container %s does not contain ROS 2 Jazzy. Recreate it with --replace.\n' "$name" >&2
        exit 1
    fi
}

ensure_container "$SIM_CONTAINER"
ensure_container "$GUI_CONTAINER"

printf 'Installing ROS 2 Jazzy, Gazebo Harmonic, and container utilities...\n'
for container in "$SIM_CONTAINER" "$GUI_CONTAINER"; do
    docker exec --user 0:0 "$container" bash -lc '
        set -Ee -o pipefail
        # Rootless/user-namespaced Docker cannot always let apt drop to its
        # _apt sandbox account. Keep apt inside the container running as root.
        printf "%s\n" "APT::Sandbox::User \"root\";" > /etc/apt/apt.conf.d/99-container-apt-sandbox
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            ca-certificates \
            git \
            python3-colcon-common-extensions \
            python3-yaml \
            ros-dev-tools \
            sudo \
            wget \
            xauth
        rm -rf /var/lib/apt/lists/*
    '
done

printf 'Installing PX4 build and Gazebo dependencies...\n'
docker exec --user 0:0 --workdir /workspace "$SIM_CONTAINER" bash -lc '
    set -Ee -o pipefail
    export RUNS_IN_DOCKER=true
    cd /workspace/worktrees/autopilot/autopilot
    PIP_CONSTRAINT=/workspace/worktrees/simulation/Scripts/px4-pip-constraints.txt \
        Tools/setup/ubuntu.sh --no-nuttx
    rm -f /etc/apt/apt.conf.d/99-container-apt-sandbox
'

# The PX4 installer above adds the OSRF repository and installs Gazebo
# Harmonic in the SITL container. The GUI container uses the same Gazebo
# environment but needs its own executable and runtime packages. Install the
# upstream Gazebo package directly; ros-jazzy-ros-gzharmonic is not available
# from all Jazzy apt mirrors and is not needed by this ROS workspace.
printf 'Installing Gazebo Harmonic in the GUI container...\n'
docker exec --user 0:0 "$GUI_CONTAINER" bash -lc '
    set -Ee -o pipefail
    printf "%s\n" "APT::Sandbox::User \"root\";" > /etc/apt/apt.conf.d/99-container-apt-sandbox
    install -d -m 0755 /usr/share/keyrings
    wget -q https://packages.osrfoundation.org/gazebo.gpg \
        -O /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg
    chmod 0644 /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(. /etc/os-release && echo "$VERSION_CODENAME") main" \
        > /etc/apt/sources.list.d/gazebo-stable.list
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends gz-harmonic
    rm -rf /var/lib/apt/lists/*
    rm -f /etc/apt/apt.conf.d/99-container-apt-sandbox
'

printf 'Installing Micro XRCE-DDS Agent %s...\n' "$MICRO_XRCE_AGENT_VERSION"
docker exec --user 0:0 \
    --env "MICRO_XRCE_AGENT_VERSION=${MICRO_XRCE_AGENT_VERSION}" \
    --env "PX4_BUILD_WORKERS=${PX4_BUILD_WORKERS}" \
    "$SIM_CONTAINER" bash -lc '
        set -Ee -o pipefail
        if ! command -v MicroXRCEAgent >/dev/null 2>&1; then
            source_dir="/tmp/Micro-XRCE-DDS-Agent-${MICRO_XRCE_AGENT_VERSION}"
    if [[ ! -d "$source_dir/.git" ]]; then
                git clone --depth 1 --branch "$MICRO_XRCE_AGENT_VERSION" \
                    https://github.com/eProsima/Micro-XRCE-DDS-Agent.git "$source_dir"
            fi
            cmake -S "$source_dir" -B "$source_dir/build" -DCMAKE_BUILD_TYPE=Release
            cmake --build "$source_dir/build" --parallel "${PX4_BUILD_WORKERS}"
            cmake --install "$source_dir/build"
            ldconfig
        fi
    '

    if [[ ! -x "$PX4_DIR/build/px4_sitl_default/bin/px4" ]]; then
    printf 'Building PX4 SITL with Gazebo Harmonic...\n'
    docker exec --user "${SIM_UID}:${SIM_GID}" \
        --env HOME=/tmp \
        --env CCACHE_DIR=/tmp/uavcomms-ccache \
        --env "PX4_BUILD_WORKERS=${PX4_BUILD_WORKERS}" \
        --workdir /workspace/worktrees/autopilot/autopilot \
        "$SIM_CONTAINER" bash -lc '
            set -Ee -o pipefail
            # The `gz_x500` make target launches a persistent simulator.
            # Provisioning only needs the SITL binary; launch it separately.
            make px4_sitl_default -j"${PX4_BUILD_WORKERS}"
        '
else
    printf 'Using existing PX4 SITL build at %s/build/px4_sitl_default.\n' "$PX4_DIR"
fi

SIM_CONTAINER="$SIM_CONTAINER" \
SIM_UID="$SIM_UID" \
SIM_GID="$SIM_GID" \
BUILD_WORKERS="$BUILD_WORKERS" \
    bash "$PROJECT_ROOT/worktrees/simulation/Scripts/build-workspace.sh"

printf '\nSetup complete. Start the simulation with ./Scripts/restart-simulation.sh from worktrees/simulation.\n'
