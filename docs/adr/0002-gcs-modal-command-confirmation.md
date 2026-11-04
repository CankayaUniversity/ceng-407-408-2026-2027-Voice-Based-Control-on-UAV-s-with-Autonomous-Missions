# GCS Pop-up Modal Command Confirmation

## Context
Voice recognition and natural language parsing are probabilistic and susceptible to acoustic noise, homophones, or LLM hallucinations. Misparsed commands sent directly to UAVs can cause dangerous or unintended flight maneuvers.

## Decision
All parsed voice commands require explicit human confirmation via a modal pop-up on the GCS interface presenting the command details with Accept and Decline buttons before any MAVLink command is dispatched. Voice-based confirmation or selective bypasses for specific command types may be considered as future enhancements.

## Why
A mandatory visual confirmation pop-up provides an unambiguous human-in-the-loop safety barrier with negligible engineering complexity in early phases, preventing rogue flight executions without depending on unverified voice confirmation loops.
