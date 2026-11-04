# Local-Only Offline Processing

## Context
Deploying autonomous UAV missions in the field often occurs in remote, GPS-degraded, or network-constrained environments where external internet connectivity cannot be assumed. Cloud-hosted STT and LLM services introduce latency, cost, and reliability bottlenecks.

## Decision
All subsystems—including audio filtering, speech-to-text, intent parsing, swarm coordination, and flight management—must execute strictly on local machine hardware (GCS workstation and UAV compute) without relying on any external cloud APIs or active internet connections.

## Why
Offline local processing guarantees mission availability under complete network isolation, removes telemetry and audio data leakage risks, and ensures predictable, low-latency execution essential for real-time UAV operations.
