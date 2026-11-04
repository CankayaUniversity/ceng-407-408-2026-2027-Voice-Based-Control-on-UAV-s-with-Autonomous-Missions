# In-Process Voice Module Embedded in QGroundControl

## Context
We need to determine how the voice processing pipeline (audio capture, STT, intent matching) integrates with the Ground Control Station. The two main architectural alternatives are running an external background daemon communicating via local IPC (e.g. WebSocket/sockets) versus embedding the voice engine directly into the QGroundControl (QGC) binary.

## Decision
Embed the voice engine directly inside the QGroundControl application as a native C++ subsystem/library compiled into the QGC binary, rather than running a separate standalone process.

## Why
A unified binary eliminates multi-process lifecycle management, IPC latency, port/socket configuration, and external process failure modes for ground operators. It leverages QGC's existing Qt event loop, threading primitives, and in-memory audio buffers from `AudioCapture`. C++ inference runtimes (such as `whisper.cpp` or `sherpa-onnx`) make in-process offline STT performant and native.
