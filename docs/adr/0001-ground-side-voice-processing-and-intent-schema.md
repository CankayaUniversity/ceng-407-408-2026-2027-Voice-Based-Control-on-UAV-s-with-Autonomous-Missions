# Ground-Side Voice Processing and Decoupled Intent Schema

## Context
Operators command autonomous UAVs using speech. We must choose where voice recognition executes (ground vs onboard UAV) and how spoken instructions translate into autopilot commands (direct MAVLink mapping vs intermediate intent representation).

## Decision
Run Speech-to-Text (STT) and Natural Language Understanding (NLU) on the Ground Control Station (GCS), decoupling spoken inputs from flight hardware via an intermediate `Mission Intent` schema before translating into MAVLink commands.

## Why
Onboard speech recognition introduces severe compute, weight, and power penalties on UAVs and complicates multi-vehicle coordination. An intermediate `Mission Intent` schema isolates NLP variance from flight mechanics, enabling validation, multi-drone mission planning, safety/failsafe verification, and operator confirmation before low-level MAVLink messages are dispatched.
