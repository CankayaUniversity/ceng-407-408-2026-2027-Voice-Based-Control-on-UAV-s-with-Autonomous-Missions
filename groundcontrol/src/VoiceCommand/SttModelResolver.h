#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "SpeechToTextInterface.h"

/// \brief Metadata descriptor for a dynamically discovered STT model on disk.
struct SttModelDescriptor
{
    QString id;                                         ///< File or directory name (e.g. "ggml-base.en.bin")
    QString displayName;                                ///< Human-readable label shown in Settings ComboBox
    QString absoluteFilePath;                           ///< Resolved absolute path on disk
    QStringList supportedLanguages;                     ///< Inferred language support ("en", "tr")
    SttEngineType engineType = SttEngineType::Whisper;  ///< Automatically inferred runtime engine type
};

/// \brief Stateless cross-platform & Debug STT model scanner and path resolver.
///
/// Scans the 5-stage search cascade for valid STT model files/directories
///   1. User-specified custom model file or directory (sttCustomModelPath)
///   2. QGC_STT_MODEL_DIR environment variable
///   3. Executable-relative directory
///   4. Build/Source-tree directory
///   5. OS application data directory
class SttModelResolver
{
public:
    SttModelResolver() = delete;

    /// Maps the VoiceCommandSettings::sttLanguage enum value (0=English, 1=Turkish) to ISO code ("en", "tr").
    static QString languageCodeFromSettingIndex(int languageIndex);

    /// Returns the ordered list of candidate search directories (stages 2 through 5, plus custom dir if directory).
    static QStringList searchDirectories(const QString& customModelPath = QString(),
                                         const QString& appDirPathOverride = QString(),
                                         const QString& envDirOverride = QString(), bool isolateToOverrides = false);

    /// Dynamically scans all search directories in priority order and returns all discovered STT models.
    /// Deduplicates by filename so higher-priority directories shadow lower-priority ones.
    static QList<SttModelDescriptor> discoverModels(const QString& customModelPath = QString(),
                                                    const QString& appDirPathOverride = QString(),
                                                    const QString& envDirOverride = QString(),
                                                    bool isolateToOverrides = false);

    /// Builds a SttModelDescriptor for a single model file or directory path if valid.
    static bool inspectModelFileOrDir(const QString& path, SttModelDescriptor& outDescriptor);

    /// Resolves the best matching model descriptor for the requested model ID/filename (or first compatible model).
    /// Returns false if no STT models exist on disk.
    static bool resolveModel(const QString& selectedModelIdOrName, const QString& languageCode,
                             SttModelDescriptor& outDescriptor, const QString& customModelPath = QString(),
                             const QString& appDirPathOverride = QString(), const QString& envDirOverride = QString(),
                             bool isolateToOverrides = false);

    /// Resolves a complete SttConfig ready for SpeechToTextInterface::initialize().
    /// If no model is found on disk, the returned SttConfig has an empty modelPath.
    static SttConfig resolveConfig(int languageSettingIndex, const QString& selectedModelIdOrName,
                                   const QString& customModelPath, bool useGpu,
                                   const QString& appDirPathOverride = QString(),
                                   const QString& envDirOverride = QString(), bool isolateToOverrides = false);
};
