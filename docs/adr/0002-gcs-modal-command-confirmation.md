# GCS Pop-up Modal Command Confirmation

## Context
Voice recognition and natural language parsing are probabilistic and susceptible to acoustic noise, homophones, or LLM hallucinations. Misparsed commands sent directly to UAVs can cause dangerous or unintended flight maneuvers.

## Decision
All parsed voice commands require explicit human confirmation via a modal pop-up on the GCS interface presenting the command details with Accept and Decline buttons before any MAVLink command is dispatched. For multi-step command sequences, all steps default to active (selected), and the operator can toggle individual steps off or back on before approving:
- Active steps are dynamically re-numbered and the header reflects the active step count.
- Target vehicle identity is shown once in the dialog header when all steps target the same vehicle (falling back to per-step tags only for mixed-vehicle sequences).
- Upon approval, `VoiceCommandManager` filters the pending intent list to only the active steps in order and emits `commandApproved` with the filtered sequence; approval is disabled if zero steps remain active.

Voice-based confirmation or selective bypasses for specific command types may be considered as future enhancements.

## Why
A mandatory visual confirmation pop-up provides an unambiguous human-in-the-loop safety barrier with negligible engineering complexity in early phases, preventing rogue flight executions without depending on unverified voice confirmation loops. Allowing per-step toggling lets the operator drop an unwanted or redundant step (for example, an already-satisfied `takeoff`) without discarding and re-speaking the entire command sequence.
