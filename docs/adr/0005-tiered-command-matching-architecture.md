# Tiered Command Matching Architecture (Fast Deterministic Baseline with Local LLM Evolution)

## Context
Voice commanding demands immediate response times for standard maneuvers (e.g., "take off", "land", "hold"), but simple pattern matching lacks the expressiveness to understand complex mission instructions or multi-step natural language intents.

## Decision
Adopt a tiered command matching pipeline:
1. **Tier 1 (Phase 1 Baseline)**: Fast deterministic grammar/regex pattern matching running in <10ms for standardized flight commands and slot extraction.
2. **Tier 2 (Phase 2 & Beyond)**: Fallback to an offline local SLM/LLM (e.g., via MCP tool calling or structured JSON schema output) to parse ambiguous, compositional, or unformatted complex mission queries.

## Why
This decoupled tiered approach gives the best of both worlds: zero-latency, deterministic execution for core flight controls without waiting on LLM inference, while preserving an evolutionary path to natural language understanding for sophisticated autonomous operations.
