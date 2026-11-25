# MAVLink Command ACK Strict Acceptance Policy (`MAV_RESULT_IN_PROGRESS` Handling)

## Context
In the sequential voice command runner (`VoiceCommandQueueRunner`, Issue #46 / PR #108), commands that do not trigger an immediate flight-mode transition—such as `ChangeAltitudeCommand` and `ChangeHeadingCommand` dispatching `MAV_CMD_DO_REPOSITION`—verify their post-condition via `Command::watchForAck()` listening to `Vehicle::mavCommandResult`.

The MAVLink protocol defines `MAV_RESULT_IN_PROGRESS` as an intermediate acknowledgement for long-running commands before a terminal `MAV_RESULT_ACCEPTED` or failure result is sent. While PX4 currently returns `MAV_RESULT_ACCEPTED` directly for `MAV_CMD_DO_REPOSITION`, it is not guaranteed that all future commands or PX4 firmware versions will avoid emitting `MAV_RESULT_IN_PROGRESS`. Treating `MAV_RESULT_IN_PROGRESS` as `true` (accepted) would be unsafe because the command could still fail afterward.

## Decision
1. **Strict Terminal Acceptance**: `Command::watchForAck()` only marks a command as acknowledged when `result == MAV_RESULT_ACCEPTED` and `failureCode == Vehicle::MavCmdResultCommandResultOnly`.
2. **Fail-Closed on Non-Accepted Results**: Any other `MAV_RESULT`—including `MAV_RESULT_IN_PROGRESS`—currently marks the command as failed (`_ackFailed = true`) and disconnects the ACK listener. We explicitly choose **not** to treat `MAV_RESULT_IN_PROGRESS` as accepted.
3. **Deferred Multi-Stage ACK Support**: If a future voice command or PX4 firmware update emits `MAV_RESULT_IN_PROGRESS` in practice, we will revisit `Command::watchForAck()` and design a dedicated multi-stage ACK mechanism (keeping the listener active until a terminal result arrives and coordinating with the post-condition watchdog).

## Why
Failing closed keeps the Phase 1 ACK logic minimal, deterministic, and safe against false-positive command acceptance, while recording the `MAV_RESULT_IN_PROGRESS` trade-off so it can be addressed cleanly if a command requires it later.
