# Pluggable Speech-to-Text Interface with Whisper C++ Backend

## Context
Offline speech-to-text engines vary in latency, hardware acceleration (CPU vs GPU), memory footprint, and model formats. Tightly coupling the voice subsystem to a single inference library would prevent upgrading to faster or lighter models in the future.

## Decision
Introduce an abstract C++ interface (`SpeechToTextInterface`) in QGroundControl that defines the contract for taking in-memory PCM audio buffers (16 kHz Mono Int16) and returning text transcripts. Implement `whisper.cpp` as the initial baseline backend while allowing alternative runtimes (such as `faster-whisper` C++ bindings or ONNX runtimes) to be plugged in seamlessly.

## Why
An interface layer mirrors QGC's existing modular design (such as `NoiseFilterInterface`), isolates third-party ML headers, and enables benchmarking and swapping STT backends across CPU and GPU hardware without changing any upstream voice logic.
