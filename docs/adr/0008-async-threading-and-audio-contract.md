# Asynchronous Threading and In-Memory Audio Contract

## Context
Running STT inference on QGroundControl's main Qt event thread would cause UI freezes and stuttering video/telemetry feeds. Additionally, the STT engine requires a specific audio format, whereas hardware microphones may negotiate different sample rates.

## Decision
1. **Asynchronous Worker Thread**: All STT inference and deterministic command parsing execute on a dedicated background worker thread (`QThread`), communicating with the main thread via Qt signals/slots to trigger GUI pop-ups.
2. **Audio Contract**: Standardize on 16 kHz Mono Int16 PCM. `AudioCapture` targets 16 kHz Mono Int16 by default; if hardware requires a different preferred format, a software resampler downmixes the audio to 16 kHz Mono before passing it to the STT worker.

## Why
This keeps the QGC UI responsive at 60 FPS under all operational conditions and shields the speech inference pipeline from hardware microphone idiosyncrasies.
