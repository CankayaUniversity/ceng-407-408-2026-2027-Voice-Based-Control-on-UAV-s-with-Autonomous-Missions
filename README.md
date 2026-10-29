# Voice Based Control on UAV/s with Autonomous Missions

---

## Repository Architecture & Git Worktree Workflow

This repository uses a **branch-per-subsystem** architecture. Each major subsystem resides on its own branch (`<subsystem>/master`), while `main` serves as the orchestration and root branch.

To work across all components simultaneously, worktrees are created in the sibling directory (`../worktrees/`) alongside the main repository clone.

### Subsystems & Branches

| Subsystem | Branch | Worktree Path | Description |
|-----------|--------|---------------|-------------|
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
│   │   └── setup_worktrees.sh          <-- Setup script
│   ├── Tools/
│   │   └── uav-missions.code-workspace <-- VS Code multi-root workspace file
│   └── README.md
│
└── worktrees/                           <-- Sibling directory for all worktrees
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
