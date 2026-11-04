# Voice-Based Control on UAVs with Autonomous Missions

Context for voice-commanded autonomous UAV operations, mission planning, and multi-vehicle coordination.

## Language

**Autopilot**:
The onboard flight control system (running PX4 firmware) executing low-level state estimation, flight modes, and actuator control.
_Avoid_: Flight computer, drone controller

**Command Confirmation**:
A modal pop-up on the GCS displaying the parsed command details, requiring explicit operator action (Accept or Decline) before dispatching commands to the autopilot.
_Avoid_: Auto-dispatch, silent execution, unconfirmed execution

**Command Sequence**:
An ordered series of flight commands parsed from a single compound utterance, executed sequentially with abort-on-failure behavior.
_Avoid_: Composite command, batch command, multi-command

**Core Command Set**:
The standardized baseline vocabulary of single-drone flight commands (`ARM`, `DISARM`, `TAKEOFF`, `LAND`, `HOLD`, `MOVE`, `RETURN_TO_LAUNCH`) supported in Phase 1.
_Avoid_: Low-level commands, custom action strings

**Deterministic Matcher**:
A rule- and pattern-based parser that maps speech transcripts directly into structured Mission Intents with sub-10ms latency without stochastic language models.
_Avoid_: Hardcoded parser, keyword trigger

**Ground Control Station (GCS)**:
The ground-based computing workstation and operator interface responsible for telemetry monitoring, speech recognition, intent parsing, and mission oversight.
_Avoid_: Base station, ground client, remote controller

**HUD Notification (Toast)**:
A transient, non-blocking on-screen banner providing visual feedback on unrecognized spoken utterances or system status without interrupting the operator workflow.
_Avoid_: Error pop-up, blocking alert

**In-Process Voice Engine**:
The voice processing module embedded directly within the QGroundControl binary as a C++ subsystem, executing STT and command parsing in-process without external background daemons.
_Avoid_: Voice daemon, external voice service, IPC voice bridge

**Local-Only Processing**:
The architectural constraint requiring all audio capture, speech recognition, intent parsing, and flight control to execute strictly on local machine hardware with zero external cloud dependencies.
_Avoid_: Cloud AI, hybrid cloud, remote API

**Mission Intent**:
A structured, intermediate semantic representation of an operator's voice command specifying target vehicle IDs, action types, spatial/temporal parameters, and confirmation flags prior to translation into MAVLink commands.
_Avoid_: Raw command, speech payload, voice packet

**MoSCoW Prioritization**:
The requirement classification framework categorizing capabilities into Must-Have [M], Should-Have [S], Could-Have [C], and Won't-Have [W].
_Avoid_: Priority levels, tier grading

**Offboard Control**:
A PX4 flight mode where external velocity setpoints and acceleration feed-forward values are continuously streamed to govern vehicle movement.
_Avoid_: RC override, manual passthrough

**Post-condition**:
A flight telemetry condition (e.g. target altitude achieved within 3000ms) evaluated after command dispatch to confirm successful execution.
_Avoid_: Completion test, success criteria

**Pre-condition**:
A prerequisite vehicle state (e.g. armed status, GPS fix, flight mode) that must be satisfied before a confirmed command is permitted to execute.
_Avoid_: Safety guard, prerequisite check

**Push-to-Talk (PTT)**:
An audio acquisition method where voice recording is explicitly triggered by operator action (e.g., keypress or UI button), capturing bounded audio processed through DSP noise filtering.
_Avoid_: Always-listening, hotword activation, wake-word loop

**Speech-to-Text Interface (`SpeechToTextInterface`)**:
An abstract C++ interface providing an extensible middle layer for offline speech recognition backends (e.g., `whisper.cpp`, ONNX runtimes), decoupling audio transcription from specific ML inference engines.
_Avoid_: Hardcoded STT backend, monolithic transcriber

**Swarm Controller**:
A distributed controller employing Model Predictive Control (MPC) to generate collision-free velocity and acceleration setpoints across UAVs in formation or non-formation flight.
_Avoid_: Centralized autopilot, swarm master

**VoiceCommand Subsystem**:
The native C++ module residing in `groundcontrol/src/VoiceCommand/` encapsulating audio ingestion, speech-to-text inference, deterministic intent matching, and confirmation dialog management.
_Avoid_: Voice plugin, speech script, external voice daemon
