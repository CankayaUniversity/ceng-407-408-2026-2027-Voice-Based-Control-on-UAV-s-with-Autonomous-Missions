# Tiered Command Matching and Context-Aware Intent Parsing Architecture

## Context
Voice commanding demands immediate response times for standard maneuvers (e.g., "take off", "land", "hold"), but simple pattern matching lacks the expressiveness to understand complex mission instructions or multi-step natural language intents. Furthermore, some spoken instructions represent **atomic primitive commands** (e.g., "move 10 meters forward") that must be rejected if the vehicle is disarmed on the ground, whereas others represent **high-level mission goals** (e.g., "start perimeter surveillance") that implicitly require preparatory steps (`ARM` -> `TAKEOFF` -> `START_SURVEILLANCE`) when issued while disarmed.

## Decision
1. **Single Abstract Parser Seam (`IntentParserInterface`)**: Decouple `VoiceCommandManager` from concrete parsing implementations via a single `IntentParserInterface` contract (`parse(const QString& transcript, const VehicleStateSnapshot& vehicleState)`), returning an `IntentParseResult` containing an ordered `QList<MissionIntent>` (`Command Sequence`). `VoiceCommandManager` holds and interacts with only one `IntentParserInterface` instance (mirroring `SpeechToTextInterface`) and remains agnostic to the internal parsing strategy.
2. **Pluggable Parsing Strategies (Phase 1 Baseline vs. Phase 2+ NLU)**:
   - **Phase 1 Baseline (`DeterministicMatcher`)**: Fast deterministic grammar/regex pattern matching (<10ms) implementing `IntentParserInterface` to validate the Phase 1 end-to-end pipeline and provide a lightweight test/baseline matcher.
   - **Phase 2 & Beyond (NLU / Local LLM / ONNX)**: Offline local SLM/LLM or ONNX NLU intent extractors implementing `IntentParserInterface` to parse natural, ambiguous, or compositional Turkish/English mission queries. Whether Phase 2+ runs as a **standalone** NLU/LLM parser or uses a **tiered/fallback composition** (e.g., a `TieredIntentParser` implementing `IntentParserInterface` that delegates to deterministic matching first and falls back to an LLM) is an optional implementation choice inside the parser layer—not a requirement on `VoiceCommandManager`.
3. **Context-Aware Goal Expansion vs. Atomic Pre-Conditions**:
   - **Atomic Commands** (`MOVE`, `CHANGE_ALTITUDE`, `CHANGE_HEADING`, `LAND`, `HOLD`, `RTL`) map 1-to-1 to a single `MissionIntent`. If issued in an invalid vehicle state (e.g., `MOVE` while disarmed), they are not auto-expanded; instead, they fail the execution engine's pre-condition check (`REQ-CORE-018`).
   - **Composite / High-Level Goal Intents** inspect `VehicleStateSnapshot` (`isArmed`, `isFlying`, `flightMode`) during parsing and expand into an explicit multi-step `Command Sequence` (e.g., `[ARM, TAKEOFF, <GOAL>]` if disarmed), presenting the full expanded sequence in the Operator Confirmation modal (`REQ-CORE-010`) before execution.

## Why
Keeping `VoiceCommandManager` coupled only to `IntentParserInterface` gives Phase 1 a fast, deterministic baseline while allowing Phase 2+ to adopt either a pure NLU/LLM parser or a composite tiered parser without modifying the manager, UI, or command execution pipeline.
