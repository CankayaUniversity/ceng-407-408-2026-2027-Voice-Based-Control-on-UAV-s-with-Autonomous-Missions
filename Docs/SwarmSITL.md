# UAVComms swarm SITL

> **Prototype only:** This code uses a central swarm registry and
> Reynolds-style local avoidance. It does **not** implement the leaderless,
> gossip-based distributed MPC swarm controller specified by the main
> repository's ADR-0012 and glossary.

Run from `worktrees/simulation` after creating all worktrees with the main
repository's `./Scripts/setup_worktrees.sh`:

```bash
./Scripts/setup-containers.sh
./Scripts/restart-simulation.sh
../swarm/Scripts/create-swarm.sh id:1
../swarm/Scripts/conscript-drone.sh id:0 swarm_id:1
../swarm/Scripts/go-mission.sh x:1 y:2 z:3 swarm_id:1
../swarm/Scripts/list-drones.sh
./Scripts/stop-simulation.sh
```

`setup-containers.sh` mounts the parent directory of `worktrees` into both
containers at `/workspace`, builds the pinned `autopilot/master` firmware and
`agent/master` PX4 ROS 2 submodules, then builds all six `uav_comms_*` packages.
The PX4 installer uses `Scripts/px4-pip-constraints.txt` to keep its Python
dependencies compatible with the ROS Jazzy image's Debian NumPy package.
Each container also links the checkout's absolute host path to `/workspace`;
this lets PX4's CMake read the Git worktree metadata during firmware builds.
PX4 compilation uses a writable container-local ccache at
`/tmp/uavcomms-ccache` rather than the image's root-owned `/tmp/.cache`.
Setup builds the `px4_sitl_default` target without starting the persistent
Gazebo/PX4 simulation; start it separately with `restart-simulation.sh`.


```bash
docker exec --user "$(id -u):$(id -g)" swarm-drones-sitl bash -lc \
  'source /opt/ros/jazzy/setup.bash && source /workspace/install/setup.bash && cd /workspace && ROS_DOMAIN_ID=181 python3 -m unittest discover -s worktrees/swarm/Tests -v'
```

The generated `build/`, `install/`, `log/`, `simulation.log` and
`gazebo-gui.log` live in the shared parent folder outside Git worktrees.
