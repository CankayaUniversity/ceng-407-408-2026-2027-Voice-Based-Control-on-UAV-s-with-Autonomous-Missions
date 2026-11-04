# Native QGC Vehicle Dispatch for Single Drone with Future Swarm Decoupling

## Context
Once an operator approves a command, it must be translated into vehicle flight instructions. We need to decide whether to reuse QGroundControl's built-in `Vehicle` state-machine APIs or build a custom MAVLink dispatcher from scratch.

## Decision
For Phase 1 single-drone operations, dispatch approved standard flight commands (`Takeoff`, `Land`, `RTL`, `Arm`/`Disarm`) directly through QGC's native `Vehicle` methods (`Vehicle::guidedModeTakeoff`, etc.), while streaming MAVLink Offboard setpoints specifically for relative vector `MOVE` commands. In future swarm phases, multi-UAV commands will bypass single `Vehicle` methods and route through a dedicated Swarm Orchestration layer.

## Why
Reusing QGC's internal `Vehicle` flight management for single drones guarantees robust pre-flight checks, state transitions, and safety validations without duplicating MAVLink handling. Decoupling the swarm routing preserves a clean architecture for multi-UAV coordination in later phases.
