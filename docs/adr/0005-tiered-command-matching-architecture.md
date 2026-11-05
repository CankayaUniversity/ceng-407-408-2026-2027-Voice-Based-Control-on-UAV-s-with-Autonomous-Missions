# Tiered Command Matching and Context-Aware Intent Parsing Architecture

## Context
Voice commanding demands immediate response times for standard maneuvers (e.g., "take off", "land", "hold"), but simple pattern matching lacks the expressiveness to understand complex mission instructions or multi-step natural language intents. Furthermore, some spoken instructions represent **atomic primitive commands** (e.g., "move 10 meters forward") that must be rejected if the vehicle is disarmed on the ground, whereas others represent **high-level mission goals** (e.g., "start perimeter surveillance") that implicitly require preparatory steps (`ARM` -> `TAKEOFF` -> `START_SURVEILLANCE`) when issued while disarmed.

## Decision
1. **Abstract Parser Contract (`IntentParserInterface`)**: Decouple `VoiceCommandManager` from concrete parsing implementations via `IntentParserInterface::parse(const QString& transcript, const VehicleStateSnapshot& vehicleState)`, returning an `IntentParseResult` containing an ordered `QList<MissionIntent>` (`Command Sequence`).
2. **Tiered Command Matching Pipeline**:
   - **Tier 1 (Phase 1 Baseline)**: Fast deterministic grammar/regex pattern matching (`DeterministicMatcher`) running in <10ms for standardized flight commands and slot extraction.
   - **Tier 2 (Phase 2 & Beyond)**: Pluggable offline local SLM/LLM or ONNX NLU intent extractor under `IntentParserInterface` (supporting standalone or fallback chaining from Tier 1) to parse ambiguous, compositional, or natural Turkish/English mission queries.
3. **Context-Aware Goal Expansion vs. Atomic Pre-Conditions**:
   - **Atomic Commands** (`MOVE`, `CHANGE_ALTITUDE`, `CHANGE_HEADING`, `LAND`, `HOLD`, `RTL`) map 1-to-1 to a single `MissionIntent`. If issued in an invalid vehicle state (e.g., `MOVE` while disarmed), they are not auto-expanded; instead, they fail the execution engine's pre-condition check (`REQ-CORE-018`).
   - **Composite / High-Level Goal Intents** inspect `VehicleStateSnapshot` (`isArmed`, `isFlying`, `flightMode`) during parsing and expand into an explicit multi-step `Command Sequence` (e.g., `[ARM, TAKEOFF, <GOAL>]` if disarmed), presenting the full expanded sequence in the Operator Confirmation modal (`REQ-CORE-010`) before execution.

## Why
This decoupled, context-aware tiered approach provides sub-10ms deterministic execution for core flight controls while enabling seamless integration of future intent-extraction models and safe, transparent expansion of high-level mission goals.
