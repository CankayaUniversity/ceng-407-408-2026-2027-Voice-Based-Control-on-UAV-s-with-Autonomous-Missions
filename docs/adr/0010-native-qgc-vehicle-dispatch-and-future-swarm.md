# Native QGC Vehicle Dispatch, Sequential Pre/Post-Conditions, and Future Swarm Decoupling

## Context
Once an operator approves a matched command or `Command Sequence`, it must be validated and translated into vehicle flight instructions. In multi-step sequences (such as `[ARM, TAKEOFF]`), evaluating all pre-conditions statically at $t=0$ would falsely reject `TAKEOFF` because the vehicle is not yet armed before step 1 executes. Conversely, physical maneuvers like climbing to 20 meters or flying 50 meters forward take longer than 3000ms to physically complete, requiring a clear distinction between autopilot command acceptance/state-transition (`<= 3000ms` post-condition) and physical trajectory completion.

## Decision
1. **Direct Native Vehicle Coupling**: For Phase 1 single-drone operations, `VoiceCommandInterface` couples directly to QGroundControl's native `Vehicle*` instance for flight action dispatch, pre-condition checks, and post-condition evaluations.
2. **Just-in-Time Stepwise Pre-Condition Evaluation**: In a `Command Sequence`, the execution engine evaluates command $i$'s `checkPreCondition(Vehicle*)` immediately before dispatching command $i$ (after command $i-1$ has satisfied its post-condition). If any pre-condition fails, the sequence aborts immediately and triggers `"Pre-condition of the command is not satisfied"` (`REQ-CORE-018`).
3. **3000ms State-Transition Post-Conditions**: Each command with a post-condition (`REQ-CORE-014`, `REQ-CORE-016`) verifies within a 3000ms watchdog timer that the autopilot accepted the command and transitioned into the target state/mode (e.g., `vehicle->armed() == true` for `ARM`, flight mode transition / MAVLink ACK for `TAKEOFF`, `LAND`, `RTL`, `HOLD`). If 3000ms elapses without satisfying the post-condition, the command fails, remaining queued commands are aborted (`REQ-CORE-020`), and `"{X} command is failed while executing"` is emitted (`REQ-CORE-021`).
4. **Standard Flight Command Dispatch vs. Offboard Movement**:
   - Dispatch standard commands (`Arm`, `Disarm`, `Takeoff`, `Land`, `RTL`, `Hold`, `ChangeAltitude`, `ChangeHeading`) via QGC's native `Vehicle` methods.
   - Stream MAVLink Offboard velocity/acceleration setpoints (`SET_POSITION_TARGET_LOCAL_NED`) for relative vector `MOVE` commands (as established in ADR-0003).
5. **Future Swarm Routing**: In Phase 3, swarm mission commands will route through a dedicated Swarm Orchestration layer.

## Why
Evaluating pre-conditions just-in-time before each step and bounding post-conditions to deterministic 3000ms telemetry/mode transitions enables reliable multi-step `Command Sequence` execution (e.g., `ARM` -> `TAKEOFF`) without false static rejections or false timeouts during long climbs.

