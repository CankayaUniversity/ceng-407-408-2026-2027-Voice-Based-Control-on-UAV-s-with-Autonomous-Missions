# Pluggable Speech-to-Text Architecture, Model-Driven Factory, and Lifecycle Management

## Context
Offline speech-to-text engines (`whisper.cpp`, ONNX runtimes, Vosk/Kaldi) vary in latency, hardware acceleration (CPU vs GPU), memory footprint, language support (English now, Turkish next), and model formats (`.bin`, `.onnx`). Tightly coupling `VoiceCommandManager` to a single inference library would prevent experimenting with new models or engines. Conversely, exposing raw engine selection in the UI risks invalid user combinations (e.g., selecting a Vosk engine with a Whisper GGML model). Furthermore, loading STT models from disk on every Push-to-Talk release adds 200–800ms of latency, while keeping models permanently in RAM wastes memory when voice control is idle. Finally, model files must resolve seamlessly across Linux and Windows in both installed Release builds and un-installed Debug/IDE builds.

## Decision
1. **Thread-Agnostic Batch `SpeechToTextInterface`**:
   - Define a pure, synchronous C++ interface (`SpeechToTextInterface`) accepting canonical 16 kHz Mono Int16 PCM buffers (`const QByteArray&`) in batch mode upon Push-to-Talk release.
   - Configure backends via `SttConfig` (`modelPath`, `language` [`"en"`, `"tr"`], `threadCount`, `useGpu`, and optional `vocabularyHints` defaulted to empty so natural phrasing and future free-form intent extraction are not degraded by aggressive prompt bias).
   - Return structured `SttResult` outputs (`bool success`, `QString text`, `float confidence`, `qint64 inferenceTimeMs`, `QString errorMessage`).
2. **Model-Driven Engine Factory (`SttEngineFactory`) & Dynamic Model Discovery (`SttModelResolver`)**:
   - Do not hardcode model lists in source code or static settings enums. Instead, `SttModelResolver` dynamically scans the model directories for valid STT models (`*.bin`, `*.gguf`, `*.onnx`, `vosk-model-*`) and populates the `VoiceCommandSettings` model selector (`availableSttModels`) at runtime so adding a new model requires zero code changes.
   - Abstract engine selection from the operator. In `VoiceCommandSettings`, the user selects only the **Language** (`en`/`tr`) and **Speech Model**.
   - `SttEngineFactory` inspects the `SttModelDescriptor` (or file signature/manifest: `ggml-*.bin` / `.gguf` -> `WhisperEngine`, `.onnx` -> `OnnxEngine`, Kaldi directory -> `VoskEngine`) and automatically instantiates the compatible `SpeechToTextInterface` implementation.
   - Implement `WhisperEngine` (`whisper.cpp` `v1.9.5` compiled in-process as a static git submodule) as the initial baseline backend.
3. **Hardware Execution Policy (Optimized CPU + Optional GPU)**:
   - Compile `whisper.cpp` with native CPU SIMD intrinsics (AVX2/FMA on x86_64, NEON on ARM) and bound default CPU worker threads to `std::clamp(logicalCores / 2, 2, 4)` to preserve CPU headroom for QGC telemetry and video decoding.
   - Expose an optional `useGpu` setting in `VoiceCommandSettings` / `SttConfig` (with automatic CPU fallback if GPU initialization fails).
4. **Smart Memory Lifecycle (Pre-Warm on PTT Press + Idle Unload Timer)**:
   - When the operator presses the PTT trigger (`startRecording`), if the STT model is currently unloaded, `VoiceCommandManager` immediately signals the background worker thread to pre-warm (`initialize`) the model in parallel while the operator is speaking.
   - Keep the model warm in memory across consecutive utterances for zero-load-latency transcription.
   - Run a configurable **Idle Unload Timer** (e.g., 5 minutes, toggleable in `VoiceCommandSettings` via *Keep Model in Memory / Idle Timeout*) that automatically calls `unload()` on the background worker when voice control is inactive.
5. **Cross-Platform & Debug-Ready Model Path Resolution (`SttModelResolver`), Selection Management (`SttModelManager`), & Readiness Gating (`VoiceCommandManager`)**:
   - Follow Single Responsibility Principle (SRP) by splitting model discovery, selection state, and orchestration across three focused classes:
     - **`SttModelResolver`** (stateless filesystem scanner & resolver): Dynamically scans and resolves model files on Linux and Windows across the 4-stage fallback cascade (`QGC_STT_MODEL_DIR` -> `<applicationDirPath>/models/stt/` -> `<QGC_SOURCE_DIR>/models/stt/` & Debug upward traversal -> `AppLocalDataLocation/models/stt/`) without holding UI or `Fact` state.
     - **`SttModelManager`** (stateful `QObject` sub-component, parallel to `AudioDeviceManager`): Manages the discovered model list (`availableModels`), active selection (`selectedModel`, `selectedModelIndex`), Settings `Fact` persistence (`sttSelectedModel`), and availability state (`isModelAvailable`), delegating filesystem scanning to `SttModelResolver`.
     - **`VoiceCommandManager`** (coordinator): Coordinates `AudioCapture`, `AudioDeviceManager`, `SttModelManager`, and `MultiVehicleManager`, exposing `isSttModelAvailable` and `isReady` (`isMicrophoneActive && isVehicleConnected && isSttModelAvailable`). When no STT model is available (`isSttModelAvailable == false`), the Push-to-Talk button is visually disabled and triggering voice control emits `"Model for voice recognition is not found"`.

## Why
This architecture isolates third-party ML headers behind a clean C++ seam, adheres to SOLID principles by keeping filesystem scanning (`SttModelResolver`), selection/Fact state (`SttModelManager`), and high-level coordination (`VoiceCommandManager`) decoupled, allows dropping new models into `models/stt/` without modifying code, and ensures frictionless execution across Linux, Windows, Release installers, and un-packaged Debug builds.
