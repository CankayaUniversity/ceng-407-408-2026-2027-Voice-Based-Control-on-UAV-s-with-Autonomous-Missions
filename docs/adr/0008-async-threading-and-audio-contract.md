# Asynchronous Threading and In-Memory Audio Contract

## Context
Running STT inference on QGroundControl's main Qt event thread would cause UI freezes and stuttering video/telemetry feeds. Additionally, speech recognition engines (`whisper.cpp`) and single-channel DSP noise filters require a standardized audio format (16 kHz Mono Int16 PCM), whereas physical microphones negotiate diverse sample rates (44.1 kHz, 48 kHz), formats (Float32, Int16), and channels (Mono, Stereo). Furthermore, conflicts between background mic testing (`Streaming`) and Push-to-Talk (`Recording`) must be handled deterministically, and headless CI environments require testing audio pipelines without physical sound hardware.

## Decision
1. **Asynchronous Worker Thread**: All STT inference and deterministic command parsing execute on a dedicated background worker thread (`QThread`), communicating with the main Qt GUI thread via signals/slots to trigger confirmation dialogs and HUD notifications.
2. **Canonical Audio Contract at Ingestion**: Standardize strictly on 16 kHz Mono Int16 PCM. `AudioCapture` enforces this contract at the hardware ingestion boundary:
   - Any multi-channel input (e.g. stereo) is downmixed to mono immediately upon frame read before passing into `DspNoiseFilter` or level meters, preventing filter corruption across channels.
   - Non-16kHz sample rates are converted to 16 kHz Int16 via a software resampler inside `AudioCapture`.
   - All emitted signals (`recordingFinished`, `audioStreamFrameReady`) unconditionally guarantee canonical 16 kHz Mono Int16 PCM data, shielding downstream modules (`VoiceManager`, `WhisperEngine`) from hardware branching.
3. **Capture Conflict Pre-emption**: When an operator initiates voice recording (Push-to-Talk) while live streaming (e.g., settings mic test) is active, `AudioCapture` automatically pre-empts and aborts the streaming session, emits a descriptive warning notification to notify the UI, and immediately transitions to recording to ensure voice commands are never dropped.
4. **Testability Stream Seam**: `AudioCapture` encapsulates an internal audio source seam accepting a `QIODevice*` (e.g. `QBuffer` containing synthetic PCM/WAV speech clips), enabling comprehensive unit and regression testing in headless CI environments without physical audio hardware.
5. **Speech Utterance Validation Boundary**: General audio acquisition, streaming, and buffering remain encapsulated in `AudioCapture`. Speech-specific validation logic (such as enforcing minimum utterance duration >=500ms and speech energy thresholds) resides within `VoiceManager` prior to triggering STT inference.
6. **Push-to-Talk Input Modalities & Dynamic Gating**: The Push-to-Talk (PTT) recording trigger supports dual input modalities: an on-screen reactive UI button and a configurable global keyboard press-and-hold shortcut (defaulting to `'P'`). PTT UI controls are dynamically gated by microphone availability exposed by `AudioDeviceManager::isMicrophoneActive`, hiding the button when no valid input device exists. Keyboard press/release events are captured via an application event filter in `VoiceCommandManager` to maintain responsiveness regardless of UI focus.

## Why
This architecture guarantees a clean, uncorrupted audio signal for DSP filtering, keeps the QGC UI responsive at 60 FPS, enforces a strict single-format contract for STT backends, eliminates multi-module conflict bugs, and allows 100% deterministic test execution in containerized CI environments.

