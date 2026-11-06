#include "WhisperEngine.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "QGCLoggingCategory.h"
#include "whisper.h"

QGC_LOGGING_CATEGORY(WhisperEngineLog, "VoiceCommand.WhisperEngine")

namespace {

void whisperQtLogCallback(enum ggml_log_level level, const char* text, void* userData)
{
    Q_UNUSED(userData);
    if (!text) {
        return;
    }

    const QString message = QString::fromUtf8(text).trimmed();
    if (message.isEmpty()) {
        return;
    }

    switch (level) {
        case GGML_LOG_LEVEL_ERROR:
            qCWarning(WhisperEngineLog) << "[whisper.cpp]" << message;
            break;
        case GGML_LOG_LEVEL_WARN:
            qCDebug(WhisperEngineLog) << "[whisper.cpp warn]" << message;
            break;
        default:
            break;
    }
}

void installWhisperLoggerOnce()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() { whisper_log_set(whisperQtLogCallback, nullptr); });
}

}  // namespace

WhisperEngine::WhisperEngine()
{
    installWhisperLoggerOnce();
}

WhisperEngine::~WhisperEngine()
{
    unload();
}

bool WhisperEngine::initialize(const SttConfig& config)
{
    if (config.modelPath.trimmed().isEmpty()) {
        qCWarning(WhisperEngineLog) << "Cannot initialize WhisperEngine: modelPath is empty";
        return false;
    }

    const QFileInfo modelFile(config.modelPath);
    if (!modelFile.exists() || !modelFile.isFile()) {
        qCWarning(WhisperEngineLog) << "Cannot initialize WhisperEngine: model file does not exist:"
                                    << config.modelPath;
        return false;
    }

    const QString canonicalPath = modelFile.absoluteFilePath();

    if (_ctx != nullptr && _config.modelPath == canonicalPath && _config.useGpu == config.useGpu) {
        _config = config;
        _config.modelPath = canonicalPath;
        qCDebug(WhisperEngineLog) << "WhisperEngine context reused with updated config. Model:" << canonicalPath
                                  << "Language:" << _config.language << "Threads:" << _config.resolvedThreadCount();
        return true;
    }

    unload();

    const QByteArray pathUtf8 = canonicalPath.toUtf8();
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = config.useGpu;

    _ctx = whisper_init_from_file_with_params(pathUtf8.constData(), cparams);
    _activeUseGpu = config.useGpu;

    if (_ctx == nullptr && config.useGpu) {
        qCWarning(WhisperEngineLog) << "GPU initialization failed for model" << canonicalPath
                                    << "- falling back to CPU";
        cparams.use_gpu = false;
        _ctx = whisper_init_from_file_with_params(pathUtf8.constData(), cparams);
        _activeUseGpu = false;
    }

    if (_ctx == nullptr) {
        qCWarning(WhisperEngineLog) << "Failed to initialize whisper_context from model:" << canonicalPath;
        return false;
    }

    _config = config;
    _config.modelPath = canonicalPath;

    qCDebug(WhisperEngineLog) << "WhisperEngine initialized successfully. Model:" << canonicalPath
                              << "Language:" << _config.language << "Threads:" << _config.resolvedThreadCount()
                              << "GPU active:" << _activeUseGpu;
    return true;
}

void WhisperEngine::unload()
{
    if (_ctx != nullptr) {
        whisper_free(_ctx);
        _ctx = nullptr;
        _activeUseGpu = false;
        qCDebug(WhisperEngineLog) << "WhisperEngine model unloaded from memory";
    }
}

bool WhisperEngine::isLoaded() const
{
    return _ctx != nullptr;
}

SttConfig WhisperEngine::config() const
{
    return _config;
}

bool WhisperEngine::convertPcm16ToFloat(const QByteArray& pcm16kMono, std::vector<float>& outSamples, QString* errorOut)
{
    if (pcm16kMono.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("PCM audio buffer is empty");
        }
        return false;
    }

    if ((pcm16kMono.size() % static_cast<qsizetype>(sizeof(int16_t))) != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("PCM audio buffer size is not aligned to 16-bit samples");
        }
        return false;
    }

    const auto sampleCount = static_cast<size_t>(pcm16kMono.size()) / sizeof(int16_t);
    const char* pcmPtr = pcm16kMono.constData();

    outSamples.resize(sampleCount);
    constexpr float kInvInt16Scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < sampleCount; ++i) {
        int16_t sample = 0;
        std::memcpy(&sample, pcmPtr + (i * sizeof(int16_t)), sizeof(int16_t));
        outSamples[i] = static_cast<float>(sample) * kInvInt16Scale;
    }

    return true;
}

SttResult WhisperEngine::transcribe(const QByteArray& pcm16kMono)
{
    SttResult result;

    if (_ctx == nullptr) {
        result.errorMessage = QStringLiteral("WhisperEngine is not initialized with a loaded model");
        qCWarning(WhisperEngineLog) << result.errorMessage;
        return result;
    }

    std::vector<float> pcmf32;
    if (!convertPcm16ToFloat(pcm16kMono, pcmf32, &result.errorMessage)) {
        qCWarning(WhisperEngineLog) << "Invalid PCM input:" << result.errorMessage;
        return result;
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.n_threads = _config.resolvedThreadCount();
    wparams.translate = false;
    wparams.no_context = true;
    wparams.no_timestamps = true;
    wparams.single_segment = true;
    wparams.print_special = false;
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.suppress_blank = true;
    wparams.suppress_nst = true;
    wparams.temperature_inc = 0.0f;

    const QString langTrimmed = _config.language.trimmed().toLower();
    const QByteArray langUtf8 = (langTrimmed.isEmpty() ? QStringLiteral("en") : langTrimmed).toUtf8();
    wparams.language = langUtf8.constData();
    wparams.detect_language = false;

    QByteArray promptUtf8;
    if (!_config.vocabularyHints.isEmpty()) {
        promptUtf8 = _config.vocabularyHints.join(QStringLiteral(", ")).toUtf8();
        wparams.initial_prompt = promptUtf8.constData();
    } else {
        wparams.initial_prompt = nullptr;
    }

    QElapsedTimer timer;
    timer.start();

    const int rc = whisper_full(_ctx, wparams, pcmf32.data(), static_cast<int>(pcmf32.size()));
    result.inferenceTimeMs = timer.elapsed();

    if (rc != 0) {
        result.errorMessage = QStringLiteral("whisper_full failed with error code %1").arg(rc);
        qCWarning(WhisperEngineLog) << result.errorMessage;
        return result;
    }

    const int segmentCount = whisper_full_n_segments(_ctx);
    QString transcript;
    double tokenProbSum = 0.0;
    int tokenCount = 0;

    for (int i = 0; i < segmentCount; ++i) {
        const char* segText = whisper_full_get_segment_text(_ctx, i);
        if (segText != nullptr) {
            const QString segStr = QString::fromUtf8(segText);
            if (!transcript.isEmpty() && !segStr.isEmpty() && !transcript.endsWith(QLatin1Char(' ')) &&
                !segStr.startsWith(QLatin1Char(' '))) {
                transcript.append(QLatin1Char(' '));
            }
            transcript.append(segStr);
        }

        const int segTokens = whisper_full_n_tokens(_ctx, i);
        for (int t = 0; t < segTokens; ++t) {
            tokenProbSum += static_cast<double>(whisper_full_get_token_p(_ctx, i, t));
            ++tokenCount;
        }
    }

    result.text = transcript.trimmed();
    result.confidence = (tokenCount > 0) ? static_cast<float>(tokenProbSum / tokenCount) : 0.0f;

    if (result.text.isEmpty()) {
        result.success = false;
        result.errorMessage = QStringLiteral("Speech-to-text produced an empty transcript");
        qCDebug(WhisperEngineLog) << result.errorMessage << "(inferenceTimeMs:" << result.inferenceTimeMs << ")";
        return result;
    }

    result.success = true;
    qCDebug(WhisperEngineLog) << "Transcribed text:" << result.text << "Confidence:" << result.confidence
                              << "Time(ms):" << result.inferenceTimeMs;
    return result;
}

QString WhisperEngine::backendName() const
{
    return QStringLiteral("whisper.cpp");
}

SttEngineType WhisperEngine::engineType() const
{
    return SttEngineType::Whisper;
}
