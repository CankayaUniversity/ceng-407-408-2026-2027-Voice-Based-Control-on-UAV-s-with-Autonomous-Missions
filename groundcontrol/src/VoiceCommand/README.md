# LLM voice intent parsing

Voice commands are transcribed locally with Whisper, then sent to an OpenAI-compatible
chat completions endpoint for intent parsing. By default the endpoint is a **local**
Ollama server (`http://127.0.0.1:11434/v1/chat/completions`) using `qwen3:1.7b`.
Install and start Ollama, then download the model (`ollama pull qwen3:1.7b`) before
using voice commands. No rule-based fallback is used if the server is unavailable.

Set these environment variables before starting QGroundControl to use another server:

- `QGC_VOICE_LLM_URL`: full chat completions URL (for example, `https://api.openai.com/v1/chat/completions`).
- `QGC_VOICE_LLM_MODEL`: model name supported by the endpoint (default: `qwen3:1.7b`).
- `QGC_VOICE_LLM_API_KEY`: optional bearer token for authenticated endpoints.

**Privacy:** The transcript and a snapshot of the active vehicle's flight state
are sent to the configured endpoint. Use the default local endpoint if this data
must remain on the device.

The model must return JSON with an `intents` array. Responses are validated
against the supported actions and parameter ranges, and must pass the existing
operator confirmation flow. A network error, timeout, unrecognized command or
invalid response fails closed without issuing an action.

The Speech Recognition test in Application Settings → Voice Command also queries
the LLM and shows its raw response or error alongside the transcript. It never
requests command confirmation or sends a command to the vehicle.
