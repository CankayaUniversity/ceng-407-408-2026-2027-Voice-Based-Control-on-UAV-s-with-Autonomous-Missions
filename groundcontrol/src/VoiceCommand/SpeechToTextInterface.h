#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <algorithm>

/// \brief Supported Speech-to-Text runtime engine families.
/// Selected automatically by SttEngineFactory based on the model descriptor/path.
enum class SttEngineType : uint8_t
{
    Whisper = 0,  ///< GGML whisper.cpp backend (.bin)
    Onnx = 1,     ///< ONNX Runtime / Sherpa-ONNX backend (.onnx)
    Vosk = 2,     ///< Kaldi / Vosk backend
    Custom = 3    ///< Custom or mock test backend
};

/// \brief Configuration parameters for initializing and running an STT engine.
struct SttConfig
{
    QString modelPath;                        ///< Absolute filesystem path to the model file/directory
    QString language = QStringLiteral("en");  ///< Language code ("en", "tr", or "auto")
    int threadCount = 0;                      ///< CPU worker threads (0 = auto-clamped to [2, 4])
    bool useGpu = false;                      ///< Attempt GPU acceleration if supported (falls back to CPU)
    QStringList vocabularyHints;              ///< Optional domain hints; empty by default for unbiased speech

    /// Resolves effective CPU thread count bounded to preserve QGC telemetry/video headroom.
    int resolvedThreadCount() const
    {
        if (threadCount > 0) {
            return threadCount;
        }
        const int ideal = QThread::idealThreadCount();
        return std::clamp(ideal > 0 ? (ideal / 2) : 2, 2, 4);
    }
};

/// \brief Structured output returned by a batch Speech-to-Text transcription call.
struct SttResult
{
    bool success = false;        ///< True if inference succeeded and produced non-empty text
    QString text;                ///< Trimmed transcription output
    float confidence = 0.0f;     ///< Average token probability in [0.0, 1.0]
    qint64 inferenceTimeMs = 0;  ///< Wall-clock inference duration in milliseconds
    QString errorMessage;        ///< Diagnostic message when success == false
};

/// \brief Abstract, thread-agnostic C++ interface for offline batch Speech-to-Text engines.
///
class SpeechToTextInterface
{
public:
    virtual ~SpeechToTextInterface() = default;
    SpeechToTextInterface(const SpeechToTextInterface&) = delete;
    SpeechToTextInterface(SpeechToTextInterface&&) = delete;
    SpeechToTextInterface operator=(const SpeechToTextInterface&) = delete;
    SpeechToTextInterface operator=(SpeechToTextInterface&&) = delete;

    /// Loads or updates the STT model in memory according to config.
    /// Returns true if the model is ready for transcription.
    virtual bool initialize(const SttConfig& config) = 0;

    /// Releases the loaded model and frees associated RAM/VRAM resources.
    virtual void unload() = 0;

    /// Returns true if a model context is currently loaded in memory.
    virtual bool isLoaded() const = 0;

    /// Returns the active configuration.
    virtual SttConfig config() const = 0;

    /// Transcribes a complete Push-to-Talk utterance in canonical 16 kHz Mono Int16 PCM format.
    virtual SttResult transcribe(const QByteArray& pcm16kMono) = 0;

    /// Human-readable backend identifier (e.g. "whisper.cpp").
    virtual QString backendName() const = 0;

    /// Engine family identifier.
    virtual SttEngineType engineType() const = 0;
};
