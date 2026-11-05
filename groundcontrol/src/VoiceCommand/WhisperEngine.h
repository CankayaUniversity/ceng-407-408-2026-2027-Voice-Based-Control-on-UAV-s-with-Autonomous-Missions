#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>

#include "SpeechToTextInterface.h"

struct whisper_context;

Q_DECLARE_LOGGING_CATEGORY(WhisperEngineLog)

/// \brief Concrete Speech-to-Text backend powered by in-process whisper.cpp (GGML).
///
class WhisperEngine final : public SpeechToTextInterface
{
public:
    WhisperEngine();
    ~WhisperEngine() override;

    WhisperEngine(const WhisperEngine&) = delete;
    WhisperEngine& operator=(const WhisperEngine&) = delete;

    bool initialize(const SttConfig& config) override;
    void unload() override;
    bool isLoaded() const override;
    SttConfig config() const override;
    SttResult transcribe(const QByteArray& pcm16kMono) override;
    QString backendName() const override;
    SttEngineType engineType() const override;

    /// Converts canonical 16 kHz Mono Int16 PCM bytes to normalized [-1.0f, 1.0f] float samples.
    static bool convertPcm16ToFloat(const QByteArray& pcm16kMono, std::vector<float>& outSamples,
                                    QString* errorOut = nullptr);

private:
    whisper_context* _ctx = nullptr;
    SttConfig _config;
    bool _activeUseGpu = false;
};
