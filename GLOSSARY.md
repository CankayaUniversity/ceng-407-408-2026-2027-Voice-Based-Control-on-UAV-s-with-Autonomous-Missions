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

**Intent Parser Interface (`IntentParserInterface`)**:
An abstract C++ interface responsible for mapping transcribed text and a `VehicleStateSnapshot` into an ordered list of `MissionIntent` structures (`IntentParseResult`). Enables swapping or chaining Tier 1 (`DeterministicMatcher`) and Tier 2 (Local LLM/ONNX NLU) parsers without modifying upstream orchestration.
_Avoid_: Hardcoded regex parser, direct STT-to-MAVLink coupling

**Local-Only Processing**:
The architectural constraint requiring all audio capture, speech recognition, intent parsing, and flight control to execute strictly on local machine hardware with zero external cloud dependencies.
_Avoid_: Cloud AI, hybrid cloud, remote API

**Mission Intent**:
A structured, intermediate semantic representation of an operator's voice command specifying target vehicle IDs, action types, spatial/temporal parameters, and display summaries prior to translation into MAVLink commands.High-level goal intents may expand into a multi-step `Command Sequence` based on current vehicle state, whereas atomic primitive commands evaluate strict pre-conditions directly.
_Avoid_: Raw command, speech payload, voice packet

**MoSCoW Prioritization**:
The requirement classification framework categorizing capabilities into Must-Have [M], Should-Have [S], Could-Have [C], and Won't-Have [W].
_Avoid_: Priority levels, tier grading

**Offboard Control**:
A PX4 flight mode where external velocity setpoints and acceleration feed-forward values are continuously streamed to govern vehicle movement.
_Avoid_: RC override, manual passthrough

**Post-condition**:
A flight telemetry state transition or autopilot acknowledgment condition (e.g., armed state change, flight mode transition, or MAV_RESULT_ACCEPTED) evaluated within 3000ms after command dispatch to confirm successful execution initiation before advancing a `Command Sequence`.
_Avoid_: Completion test, unbounded wait

**Pre-condition**:
A prerequisite vehicle state (e.g., armed status, GPS fix, flight mode) evaluated just-in-time immediately before a command in a `Command Sequence` is executed.
_Avoid_: Static upfront guard, unverified dispatch

**Push-to-Talk (PTT)**:
An audio acquisition method where voice recording is explicitly triggered by operator action (e.g., keypress or UI button), capturing bounded audio processed through DSP noise filtering.
_Avoid_: Always-listening, hotword activation, wake-word loop

**Speech-to-Text Engine Factory (`SttEngineFactory`)**:
A model-driven factory that inspects the selected `SttModelDescriptor` (or model file format/manifest) and automatically instantiates the matching `SpeechToTextInterface` backend (`WhisperEngine`, `OnnxEngine`, etc.), abstracting runtime engine selection from the operator.
_Avoid_: Manual user engine selection, mismatched model-engine pairing

**Speech-to-Text Interface (`SpeechToTextInterface`)**:
An abstract, thread-agnostic C++ interface providing an extensible middle layer for offline batch speech recognition backends (`whisper.cpp`, ONNX runtimes), accepting canonical 16 kHz Mono Int16 PCM buffers along with `SttConfig` and returning structured `SttResult` outputs.
_Avoid_: Hardcoded STT backend, monolithic transcriber

**Swarm Controller**:
A distributed controller employing Model Predictive Control (MPC) to generate collision-free velocity and acceleration setpoints across UAVs in formation or non-formation flight.
_Avoid_: Centralized autopilot, swarm master

**VoiceCommandManager**:
The central coordinator class in `groundcontrol/src/VoiceCommand/` managing Push-to-Talk lifecycle, utterance duration gating, asynchronous worker thread (`QThread`) orchestration, STT model pre-warming and idle unloading, intent parsing handoff, confirmation modal state, and sequential command execution.
_Avoid_: VoiceManager, external voice daemon

**VoiceCommand Subsystem**:
The native C++ module residing in `groundcontrol/src/VoiceCommand/` encapsulating audio ingestion coordination, speech-to-text inference, intent parsing, confirmation dialog management, and command execution.
_Avoid_: Voice plugin, speech script, external voice daemon
