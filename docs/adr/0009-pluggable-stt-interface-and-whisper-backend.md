# Pluggable Speech-to-Text Architecture, Model-Driven Factory, and Lifecycle Management

## Context
Offline speech-to-text engines (`whisper.cpp`, ONNX runtimes, Vosk/Kaldi) vary in latency, hardware acceleration (CPU vs GPU), memory footprint, language support (English now, Turkish next), and model formats (`.bin`, `.onnx`). Tightly coupling `VoiceCommandManager` to a single inference library would prevent experimenting with new models or engines. Conversely, exposing raw engine selection in the UI risks invalid user combinations (e.g., selecting a Vosk engine with a Whisper GGML model). Furthermore, loading STT models from disk on every Push-to-Talk release adds 200–800ms of latency, while keeping models permanently in RAM wastes memory when voice control is idle. Finally, model files must resolve seamlessly across Linux and Windows in both installed Release builds and un-installed Debug/IDE builds.

## Decision
1. **Thread-Agnostic Batch `SpeechToTextInterface`**:
   - Define a pure, synchronous C++ interface (`SpeechToTextInterface`) accepting canonical 16 kHz Mono Int16 PCM buffers (`const QByteArray&`) in batch mode upon Push-to-Talk release.
   - Configure backends via `SttConfig` (`modelPath`, `language` [`"en"`, `"tr"`], `threadCount`, `useGpu`, and optional `vocabularyHints` defaulted to empty so natural phrasing and future free-form intent extraction are not degraded by aggressive prompt bias).
   - Return structured `SttResult` outputs (`bool success`, `QString text`, `float confidence`, `qint64 inferenceTimeMs`, `QString errorMessage`).
2. **Model-Driven Engine Factory (`SttEngineFactory`)**:
   - Abstract engine selection from the operator. In `VoiceCommandSettings`, the user selects only the **Language** (`en`/`tr`) and **Speech Model Profile** (or custom model path).
   - `SttEngineFactory` inspects the `SttModelDescriptor` (or file signature/manifest: `ggml-*.bin` -> `WhisperEngine`, `.onnx` -> `OnnxEngine`) and automatically instantiates the compatible `SpeechToTextInterface` implementation.
   - Implement `WhisperEngine` (`whisper.cpp` compiled in-process via CMake/CPM) as the initial baseline backend.
3. **Hardware Execution Policy (Optimized CPU + Optional GPU)**:
   - Compile `whisper.cpp` with native CPU SIMD intrinsics (AVX2/FMA on x86_64, NEON on ARM) and bound default CPU worker threads to `std::clamp(logicalCores / 2, 2, 4)` to preserve CPU headroom for QGC telemetry and video decoding.
   - Expose an optional `useGpu` setting in `VoiceCommandSettings` / `SttConfig` (with automatic CPU fallback if GPU initialization fails).
4. **Smart Memory Lifecycle (Pre-Warm on PTT Press + Idle Unload Timer)**:
   - When the operator presses the PTT trigger (`startRecording`), if the STT model is currently unloaded, `VoiceCommandManager` immediately signals the background worker thread to pre-warm (`initialize`) the model in parallel while the operator is speaking.
   - Keep the model warm in memory across consecutive utterances for zero-load-latency transcription.
   - Run a configurable **Idle Unload Timer** (e.g., 5 minutes, toggleable in `VoiceCommandSettings` via *Keep Model in Memory / Idle Timeout*) that automatically calls `unload()` on the background worker when voice control is inactive.
5. **Cross-Platform & Debug-Ready Model Path Resolution (`SttModelResolver`)**:
   - Resolve model files on Linux and Windows using a 5-stage fallback cascade:
     1. User-configured custom path in `VoiceCommandSettings`.
     2. `QGC_STT_MODEL_DIR` environment variable (used by headless CI and custom overrides).
     3. Executable-relative directory: `<applicationDirPath>/models/stt/` (portable / Release bundles).
     4. Build/Source-tree directory: `<QGC_SOURCE_DIR>/models/stt/` or upward directory traversal from the Debug build binary so running directly from Qt Creator / VS Code / CLI in Debug mode works out of the box without manual copying.
     5. OS application data directory: `QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/models/stt/"`.

## Why
This architecture isolates third-party ML headers behind a clean C++ seam, prevents invalid model-engine pairings in the UI, guarantees zero model-loading delay during active missions while reclaiming RAM when idle, and ensures frictionless execution across Linux, Windows, Release installers, and un-packaged Debug builds.
