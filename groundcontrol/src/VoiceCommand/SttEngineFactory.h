#pragma once

#include <QtCore/QString>
#include <memory>

#include "SpeechToTextInterface.h"

/// \brief Model-driven factory that resolves and instantiates the appropriate
/// SpeechToTextInterface implementation without exposing engine selection to the user.
class SttEngineFactory
{
public:
    SttEngineFactory() = delete;

    /// Infers the required STT engine family from a model file path or directory.
    static SttEngineType inferEngineTypeFromPath(const QString& modelPath);

    /// Creates a concrete SpeechToTextInterface instance for the specified engine type.
    static std::unique_ptr<SpeechToTextInterface> create(SttEngineType engineType);

    /// Infers the engine type from modelPath and creates the matching SpeechToTextInterface instance.
    static std::unique_ptr<SpeechToTextInterface> createForModelPath(const QString& modelPath);
};
