#include "SttEngineFactory.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include "QGCLoggingCategory.h"
#include "WhisperEngine.h"

QGC_LOGGING_CATEGORY(SttEngineFactoryLog, "VoiceCommand.SttEngineFactory")

SttEngineType SttEngineFactory::inferEngineTypeFromPath(const QString& modelPath)
{
    const QString trimmed = modelPath.trimmed();
    if (trimmed.isEmpty()) {
        return SttEngineType::Whisper;
    }

    const QFileInfo info(trimmed);
    const QString fileName = info.fileName().toLower();
    const QString suffix = info.suffix().toLower();

    if (suffix == QStringLiteral("onnx") || fileName.contains(QStringLiteral("onnx"))) {
        return SttEngineType::Onnx;
    }

    if (info.isDir()) {
        const QDir dir(trimmed);
        if (dir.exists(QStringLiteral("am/final.mdl")) || dir.exists(QStringLiteral("final.mdl")) ||
            fileName.startsWith(QStringLiteral("vosk-model"))) {
            return SttEngineType::Vosk;
        }
        if (dir.exists(QStringLiteral("tokens.txt"))) {
            return SttEngineType::Onnx;
        }
    }

    if (fileName.startsWith(QStringLiteral("vosk-model"))) {
        return SttEngineType::Vosk;
    }

    return SttEngineType::Whisper;
}

std::unique_ptr<SpeechToTextInterface> SttEngineFactory::create(SttEngineType engineType)
{
    switch (engineType) {
        case SttEngineType::Whisper:
            return std::make_unique<WhisperEngine>();
        case SttEngineType::Onnx:
        case SttEngineType::Vosk:
        case SttEngineType::Custom:
            qCWarning(SttEngineFactoryLog) << "Requested STT engine type" << static_cast<int>(engineType)
                                           << "is not yet registered; only WhisperEngine is available in Phase 1.";
            return nullptr;
    }

    return nullptr;
}

std::unique_ptr<SpeechToTextInterface> SttEngineFactory::createForModelPath(const QString& modelPath)
{
    const SttEngineType inferredType = inferEngineTypeFromPath(modelPath);
    return create(inferredType);
}
