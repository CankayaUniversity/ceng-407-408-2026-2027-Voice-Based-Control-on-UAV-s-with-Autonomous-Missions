# Local NLU Evaluation, Native C++ Runtime Integration, and Benchmark Policy

## Context
Phase 1 validated the end-to-end voice pipeline using `DeterministicMatcher` (regex/rule baseline). Regex matching cannot scale to natural Turkish/English phrasing, numeric/semantic parameter extraction (e.g. `"ileriye 5 hızla 20 metre git"`), STT transcription noise, composite multi-step missions, or future high-level planning. To select the optimal Phase 2 local NLU architecture without guessing, we must evaluate multiple candidate cascade parsers natively in C++ under identical offline conditions (ADR-0004, ADR-0006) without degrading QGroundControl's UI responsiveness (ADR-0008) or build maintainability.

## Decision
1. **Native C++ In-Process Evaluation Priority**:
   - Implement candidate NLU backends directly in C++ behind `IntentParserInterface` rather than relying on external cloud APIs or regex fallbacks, so latency, memory, and hardware behavior reflect real QGC deployment.
   - Gate each third-party inference runtime behind opt-in CMake flags (`QGC_ENABLE_NLU_ONNX`, `QGC_ENABLE_NLU_CT2`, `QGC_ENABLE_NLU_LLAMA`) with `EXCLUDE_FROM_ALL` so QGC builds remain modular and incremental.
   - When compiling `llama.cpp` alongside `whisper.cpp`, share a single `ggml` / `ggml-vulkan` target graph to prevent duplicate symbol conflicts.
   - *(Deferred)* Fine-tuned Joint Multi-Intent + Slot encoder training (`mDeBERTa-v3` / AGIF) and End-to-End Audio-to-Intent SLU are deferred to later phases.
2. **Async Worker Execution & Model Lifecycle Parity (Mirroring ADR-0009)**:
   - Extend `IntentParserInterface` with lifecycle methods (`initialize(const NluConfig&)`, `unload()`, `isLoaded()`, `backendName()`) and structured diagnostics (`confidence`, `inferenceTimeMs`) in `IntentParseResult`.
   - Move `IntentParserInterface::parse()` off the main Qt GUI thread onto `VoiceCommandWorker` (`QThread`), supporting background pre-warming on PTT press, configurable idle unload timeouts, and GPU acceleration with automatic CPU fallback.
   - Provide a model-driven `IntentParserFactory` and `NluModelResolver` (`models/nlu/`) parallel to `SttEngineFactory` / `SttModelResolver`.
3. **Candidate Architectures Under Comparative Evaluation**:
   - **Shared Layer**: Neuro-Symbolic deterministic C++ `GoalExpander` (HTN / `VehicleStateSnapshot` expansion) shared by all neural parsers.
   - **Candidate 1**: `GLiNER2 / GLiNER2.5` via `onnxruntime` C++ (non-autoregressive unified intent + slot + schema JSON).
   - **Candidate 2**: `Seq2Seq → UAV DSL` (`mT5`/`Flan-T5` INT8) via `CTranslate2` C++ + deterministic `DslToIntentConverter`.
   - **Candidate 3**: `Tool-Use / Extraction SLM` (`Qwen2.5-1.5B` / `xLAM-2-1B` / `NuExtract`) via `llama.cpp` C++ + GBNF constrained decoding.
   - **Candidate 4**: Regex-free `TieredIntentParser` combining the top Tier 1 (fast/OOD) and Tier 2 (composite/planning) parsers.
4. **STT-Bypass Bilingual Benchmark Harness**:
   - Maintain a version-controlled Turkish + English benchmark dataset covering five categories: (1) atomic commands, (2) parameterized commands (`speed`, `distance`, `altitude`, `heading`, `direction`, `targetVehicleId`), (3) composite multi-step missions, (4) STT-noisy transcripts, and (5) Out-of-Domain (OOD) / conversational utterances for safety rejection.
   - Execute all candidate parsers through a dedicated STT-bypass C++ benchmark runner (`IntentParserBenchmark`) recording: Intent Accuracy, Slot F1, Command Sequence Exact-Match, OOD False-Positive Rate, p50/p95 Latency (ms), and Peak RSS Memory (MB).

## Why
Evaluating candidate architectures natively in C++ on an identical STT-bypass bilingual benchmark produces objective latency, memory, accuracy, and safety-rejection metrics before committing QGC to a permanent Phase 2 NLU runtime, while optional CMake flags and `VoiceCommandWorker` isolation keep QGC builds clean and the UI responsive at 60 FPS.
