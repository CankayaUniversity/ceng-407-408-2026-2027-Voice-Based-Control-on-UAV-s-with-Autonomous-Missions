# Voice Based Control on UAV/s with Autonomous Missions

---

## Repository Architecture & Git Worktree Workflow

This repository uses a **branch-per-subsystem** architecture. Each major subsystem resides on its own branch (`<subsystem>/master`), while `main` serves as the orchestration and root branch.

To work across all components simultaneously, worktrees are created in the sibling directory (`../worktrees/`) alongside the main repository clone.

### Subsystems & Branches

| Subsystem | Branch | Worktree Path | Description |
|-----------|--------|---------------|-------------|
| **Agent** | `agent/master` | `../worktrees/agent` | Companion-side PX4 ROS 2 messages, examples & frame transforms |
| **Autopilot** | `autopilot/master` | `../worktrees/autopilot` | PX4 Autopilot firmware & flight control |
| **GCS** | `gcs/master` | `../worktrees/gcs` | QGroundControl ground station & telemetry |
| **MAVLink** | `mavlink/master` | `../worktrees/mavlink` | MAVLink protocol definitions & generator |
| **Simulation** | `simulation/master` | `../worktrees/simulation` | SITL / Gazebo simulation environments |
| **Swarm** | `swarm/master` | `../worktrees/swarm` | Multi-UAV swarm mission management |
| **Tracking** | `tracking/master` | `../worktrees/tracking` | Computer vision & target tracking |
| **Voice** | `voice/master` | `../worktrees/voice` | Speech recognition & voice command processing |

---

## Directory Layout

```text
Parent-Folder/
├── ceng-407-408-2026-2027-.../          <-- Main clone (checked out to 'main')
│   ├── .github/
│   ├── Scripts/
│   │   └── setup_worktrees.sh           <-- Setup script
│   ├── Tools/
│   │   └── uav-missions.code-workspace  <-- VS Code multi-root workspace file
│   ├── docs/
│   │   ├── adr/                         <-- Architectural decisions for both agents and us
│   │   └── agents/                      <-- Skills for agents
│   ├── AGENTS.md                        <-- Routing markdown for agents
│   └── README.md
│
└── worktrees/                           <-- Sibling directory for all worktrees
    ├── agent/                          (agent/master)
    ├── autopilot/                      (autopilot/master)
    ├── gcs/                            (gcs/master)
    ├── mavlink/                        (mavlink/master)
    ├── simulation/                     (simulation/master)
    ├── swarm/                          (swarm/master)
    ├── tracking/                       (tracking/master)
    └── voice/                          (voice/master)
```

---

## Quick Start

### 1. Setup Worktrees & Submodules

```bash
./Scripts/setup_worktrees.sh
```

### 2. Open in VS Code

Open the workspace file in VS Code:

```bash
code Tools/uav-missions.code-workspace
```

## Agent ROS 2 packages

This branch (`agent/master`) owns `px4_msgs/` and `px4_ros_com/` as pinned Git
submodules compatible with the PX4 v1.17.0-based firmware on
`autopilot/master`. They are not embedded source copies. Firmware and MAVLink
integration remain in the separate `../autopilot` worktree.

From `worktrees/agent`, initialize the packages with:

```bash
git submodule update --init -- px4_msgs px4_ros_com
```

See [PX4 ROS 2 setup and validation](docs/ros2-px4.md) for the exact revisions,
Jazzy build/test commands, known upstream lint failures, and SITL telemetry
checks.

`src/uav_comms_px4_bridge` is the swarm SITL offboard bridge. It uses this
branch's pinned, firmware-compatible `px4_msgs`
