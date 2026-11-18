# ceng-407-408-2026-2027-Voice-Based-Control-on-UAV-s-with-Autonomous-Missions
Voice Based Control on UAV/s with Autonomous Missions

## UAVComms swarm packages

`src/uav_comms_interfaces` contains ROS messages and services;
`src/uav_comms_mission_planner` contains the swarm registry and mission planner;
`src/uav_comms_swarm_agent` contains the per-vehicle agent and avoidance
controllers. `Scripts/` contains the swarm command-line helpers and `Tests/`
holds the shared ROS integration tests. Build all of
these with the companion and simulation packages using
[`../simulation/Docs/SwarmSITL.md`](../simulation/Docs/SwarmSITL.md).
