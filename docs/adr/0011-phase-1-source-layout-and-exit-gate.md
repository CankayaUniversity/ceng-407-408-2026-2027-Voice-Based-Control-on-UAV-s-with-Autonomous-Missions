# Phase 1 Source Layout and Exit Verification Gate

## Context
We need a concrete source layout inside QGroundControl for the voice subsystem and unambiguous criteria to verify Phase 1 before initiating subsequent development phases.

## Decision
1. **Module Path**: Locate all integrated voice components in `groundcontrol/src/VoiceCommand/` (including `VoiceCommandManager`, `SpeechToTextInterface`, `SttEngineFactory`, `SttModelResolver`, `WhisperEngine`, `IntentParserInterface`, `DeterministicMatcher`, `MissionIntent`, `VoiceCommandInterface`, `VoiceCommandQueueRunner`, and `VoiceConfirmationDialog.qml`).
2. **Phase 1 Exit Criteria**: Phase 1 is officially complete when:
   - All `DeterministicMatcher` unit tests pass across core commands and context-aware sequences.
   - Unit and integration tests pass for `VoiceCommandManager` and `SpeechToTextInterface` (`WhisperEngine` and mock DI seams) using benchmark audio clips.
   - An end-to-end PX4 SITL simulation test executes a spoken flight command through the confirmation modal.
3. **Subsequent Roadmap**: Upon passing this gate, Phase 2 (Local LLM / ONNX Intent Parsing via `IntentParserInterface`) and Phase 3 (Distributed Swarm MPC Control) will be developed in parallel.

## Why
A dedicated module directory keeps QGC's codebase clean and maintainable. Explicit, testable exit gates ensure a robust physical-to-virtual baseline before branching into concurrent multi-drone and NLU development.
