# Native QGC Vehicle Dispatch for Single Drone with Future Swarm Decoupling

## Context
Once an operator approves a command, it must be translated into vehicle flight instructions. We need to decide whether to reuse QGroundControl's built-in `Vehicle` state-machine APIs or build a custom MAVLink dispatcher from scratch, and whether to introduce an abstract flight vehicle interface layer above `Vehicle`.

## Decision
1. **Direct Native Vehicle Coupling**: For Phase 1 single-drone operations, `VoiceCommandInterface` couples directly to QGroundControl's native `Vehicle*` instance for flight action dispatch, pre-condition checks, and post-condition evaluations, rather than introducing an intermediary abstract flight interface. `Vehicle` is a core foundational module in QGroundControl; direct coupling avoids speculative indirection layers and leverages real PX4 telemetry and flight state machines directly.
2. **Standard Flight Command Dispatch**: Dispatch approved standard flight commands (`Takeoff`, `Land`, `RTL`, `Arm`/`Disarm`) directly through QGC's native `Vehicle` methods (`Vehicle::guidedModeTakeoff()`, etc.).
3. **Offboard Relative Movement**: Stream MAVLink Offboard velocity and acceleration setpoints (`SET_POSITION_TARGET_LOCAL_NED`) specifically for relative vector `MOVE` commands (as established in ADR-0003).
4. **Future Swarm Routing**: In subsequent multi-drone phases (Phase 3), swarm mission commands will bypass single-drone `Vehicle` methods and route through a dedicated Swarm Orchestration layer.

## Why
Reusing QGC's internal `Vehicle` flight management for single drones guarantees robust pre-flight checks, state transitions, and safety validations without duplicating MAVLink handling. Direct coupling maintains simplicity in Phase 1 while decoupling the swarm routing preserves an evolutionary path for multi-UAV coordination in later phases.

