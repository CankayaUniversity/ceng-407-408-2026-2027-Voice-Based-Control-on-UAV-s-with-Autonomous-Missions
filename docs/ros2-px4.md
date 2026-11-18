# PX4 ROS 2 companion packages

The `agent/master` branch contains companion-side ROS 2 packages compatible
with the PX4 v1.17.0-based firmware on `autopilot/master`:

```text
worktrees/
├── agent/                         # agent/master; run ROS commands here
│   ├── px4_msgs/                  # Git submodule
│   └── px4_ros_com/               # Git submodule
└── autopilot/                     # autopilot/master
    └── autopilot/                 # PX4 firmware; built separately with make
```

For the time being the packages are Git submodules, not source copies embedded
directly in the repository.

| Package | Upstream branch | Pinned commit |
| --- | --- | --- |
| [`px4_msgs`](https://github.com/PX4/px4_msgs) | `release/1.17` (`v1.17.0`) | `86d8239e962f6939e05c3737784f60c02fa884db` |
| [`px4_ros_com`](https://github.com/PX4/px4_ros_com) | `main` | `86e9aeb20e55a4673fa8a9f1c29ea06a6c5ad1af` |

The selected `px4_msgs` revision has 236 message/service definitions that are
byte-identical to the firmware's `../autopilot/autopilot/msg/*.msg`,
`../autopilot/autopilot/msg/versioned/*.msg`, and
`../autopilot/autopilot/srv/*.srv`. Versioned messages are flattened into
`px4_msgs/msg/`; the firmware's `msg/px4_msgs_old/` is not part of this
comparison. All 58 message types configured in the firmware's
`src/modules/uxrce_dds_client/dds_topics.yaml` are included.

`px4_ros_com` has no `release/1.17` branch. The pinned revision uses interfaces
present in the selected `px4_msgs`, including `SensorGps` and the
`VehicleCommand` service. Do not copy the packages from the old
`swarm-drones-px4-sitl` workspace: its newer PX4 `main` definitions do not
match this firmware.

Adding these packages does not change the project's existing MAVLink control
path or launch any control nodes. `px4_ros_com` provides examples and frame
transforms; the actual ROS 2 transport is PX4's `uxrce_dds_client` communicating
with a companion-side Micro XRCE-DDS Agent.

## Initialize the packages

Run all commands in this document from `worktrees/agent`:

```bash
git submodule update --init -- px4_msgs px4_ros_com
git submodule status -- px4_msgs px4_ros_com
```

The parent repository records exact commit IDs. The branch entries in
`.gitmodules` indicate provenance, not automatic updates. Do not use
`git submodule update --remote` without rechecking firmware compatibility.
Firmware dependencies and the existing MAVLink submodule remain separate
and unchanged on `autopilot/master`.

## Build with ROS 2 Jazzy

Use Ubuntu 24.04 with ROS 2 Jazzy installed, or a Jazzy development container.
Install colcon and rosdep if needed, and initialize rosdep once if it has not
already been configured:

```bash
sudo apt install python3-colcon-common-extensions python3-rosdep
# First-time rosdep setup only:
# sudo rosdep init
rosdep update
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths px4_msgs px4_ros_com --ignore-src --rosdistro jazzy -y

CMAKE_BUILD_PARALLEL_LEVEL=2 colcon build \
  --base-paths px4_msgs px4_ros_com \
  --packages-up-to px4_ros_com \
  --parallel-workers 2
source install/setup.bash
ros2 pkg executables px4_ros_com
```

Use these explicit `--base-paths` to restrict discovery to the companion
packages. Build the firmware separately in the `autopilot` worktree using
its existing PX4 build workflow. Root-level `build/`, `install/`, and `log/`
are ignored in the `agent` worktree.

### Package tests

```bash
colcon test \
  --base-paths px4_msgs px4_ros_com \
  --packages-select px4_msgs px4_ros_com \
  --return-code-on-test-failure
colcon test-result --verbose
```

These run the packages' registered tests, including upstream lint checks;
they do not replace a SITL transport test.

### Validation results

Before relocation to this branch, both pinned packages were built successfully
with `px4io/px4-dev-ros2:main-jazzy`, including
`vehicle_gps_position_listener`. Generated Python messages and the
`VehicleCommand` service imported successfully. The repository was mounted
read-only during validation. Relocation does not change either upstream
revision.

The unmodified `px4_ros_com` revision does not pass the Jazzy lint suite:
`copyright`, `cpplint`, `flake8`, `lint_cmake`, `pep257`, and `uncrustify`
report existing upstream issues. `xmllint` additionally failed to fetch its
external schema because the validation container had networking disabled;
the `cppcheck` cases were skipped. `px4_msgs` registers no tests at this
revision. These results are not a clean test pass, and upstream sources have
not been patched to suppress them.

A PX4 SITL transport test has not been run. The following procedure documents
that remaining integration check.

## SITL telemetry smoke test

Use SITL built from the sibling `autopilot/master` worktree, not the old
workspace's newer firmware. With its firmware submodules and simulation
dependencies installed, start it in a separate terminal:

```bash
make -C ../autopilot/autopilot px4_sitl gz_x500
```

Run a compatible Micro XRCE-DDS Agent (v2.4.3 is the baseline; do not substitute
a v3.x agent for the v2.x PX4 client):

```bash
MicroXRCEAgent udp4 -p 8888
```

With SITL and the agent running, source ROS and this workspace in another
terminal and check incoming telemetry:

```bash
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 run px4_ros_com sensor_combined_listener
```

The `vehicle_gps_position_listener` executable can also be used to verify
`SensorGps` telemetry. Use best-effort QoS for PX4 telemetry subscriptions.
For the default first SITL instance, versioned telemetry uses these names:

| Message | Topic |
| --- | --- |
| `VehicleLocalPosition` (version 1) | `/fmu/out/vehicle_local_position_v1` |
| `VehicleStatus` (version 1) | `/fmu/out/vehicle_status_v1` |

For example:

```bash
ros2 topic echo /fmu/out/vehicle_status_v1 px4_msgs/msg/VehicleStatus \
  --qos-reliability best_effort --once
```

The upstream Python offboard example uses unsuffixed subscription names.
If explicitly running it in SITL, remap
`/fmu/out/vehicle_local_position` to `/fmu/out/vehicle_local_position_v1`
and `/fmu/out/vehicle_status` to `/fmu/out/vehicle_status_v1`.
Additional SITL instances may also require namespace remapping.
Offboard examples issue arming and movement commands: do not use them as
telemetry-only checks or launch them against real hardware.
