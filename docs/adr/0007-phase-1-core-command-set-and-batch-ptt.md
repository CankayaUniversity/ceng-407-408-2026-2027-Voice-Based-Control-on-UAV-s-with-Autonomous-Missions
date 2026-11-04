# Phase 1 Core Command Set and Batch PTT Processing

## Context
Phase 1 focuses on achieving ultra-fast, robust voice commanding for a single drone. We must define the audio handoff mechanism and the initial command vocabulary for the deterministic parser.

## Decision
1. **Batch PTT Handoff**: Audio captured by `AudioCapture` while holding Push-to-Talk is buffered in memory and dispatched in a single batch to the STT worker thread upon button release or VAD silence detection.
2. **Phase 1 Core Command Set**: Restrict parsing strictly to six fundamental flight commands: `ARM`/`DISARM`, `TAKEOFF [altitude]`, `LAND`, `HOLD`/`HOVER`, relative `MOVE <direction> [distance]`, and `RETURN_TO_LAUNCH` (RTL).

## Why
Batching the audio upon PTT release provides full acoustic context to the STT model, maximizing transcription accuracy while eliminating the complexity of partial-transcript state machines. Restricting Phase 1 to the core command set validates the entire pipeline (*AudioCapture -> STT -> Deterministic Matcher -> Confirmation Dialog -> MAVLink*) with deterministic low latency before expanding into compound intents.
