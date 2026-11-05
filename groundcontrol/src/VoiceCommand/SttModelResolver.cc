#include "SttModelResolver.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>

#include "QGCLoggingCategory.h"
#include "SttEngineFactory.h"

QGC_LOGGING_CATEGORY(SttModelResolverLog, "VoiceCommand.SttModelResolver")

namespace {

void appendUniqueNormalizedDir(QStringList& dirs, const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
    if (!cleaned.isEmpty() && !dirs.contains(cleaned)) {
        dirs.append(cleaned);
    }
}

QString engineTypeLabel(SttEngineType type)
{
    switch (type) {
        case SttEngineType::Whisper:
            return QStringLiteral("Whisper");
        case SttEngineType::Onnx:
            return QStringLiteral("ONNX");
        case SttEngineType::Vosk:
            return QStringLiteral("Vosk");
        case SttEngineType::Custom:
            return QStringLiteral("Custom");
    }
    return QStringLiteral("STT");
}

QStringList inferSupportedLanguagesFromName(const QString& fileNameLower)
{
    // Models ending with .en.bin / -en.onnx or containing _en_ are English-only;
    // others are treated as multilingual (supporting both English and Turkish).
    if (fileNameLower.contains(QStringLiteral(".en.")) || fileNameLower.contains(QStringLiteral("-en-")) ||
        fileNameLower.contains(QStringLiteral("_en_")) || fileNameLower.endsWith(QStringLiteral("-en"))) {
        return {QStringLiteral("en")};
    }
    if (fileNameLower.contains(QStringLiteral(".tr.")) || fileNameLower.contains(QStringLiteral("-tr-")) ||
        fileNameLower.contains(QStringLiteral("turkish"))) {
        return {QStringLiteral("tr")};
    }
    return {QStringLiteral("en"), QStringLiteral("tr")};
}

}  // namespace

QString SttModelResolver::languageCodeFromSettingIndex(int languageIndex)
{
    switch (languageIndex) {
        case 1:
            return QStringLiteral("tr");
        case 0:
        default:
            return QStringLiteral("en");
    }
}

QStringList SttModelResolver::searchDirectories(const QString& appDirPathOverride,
                                                const QString& envDirOverride, bool isolateToOverrides)
{
    QStringList dirs;

    // Stage 1: QGC_STT_MODEL_DIR environment variable
    const QString envDir = !envDirOverride.isEmpty()
                               ? envDirOverride
                               : (isolateToOverrides ? QString() : qEnvironmentVariable("QGC_STT_MODEL_DIR"));
    if (!envDir.trimmed().isEmpty()) {
        appendUniqueNormalizedDir(dirs, envDir);
    }

    // Stage 2: Executable-relative directory (<applicationDirPath>/models/stt)
    const QString appDir =
        !appDirPathOverride.isEmpty()
            ? appDirPathOverride
            : (isolateToOverrides
                   ? QString()
                   : (QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString()));
    if (!appDir.isEmpty()) {
        appendUniqueNormalizedDir(dirs, QDir(appDir).filePath(QStringLiteral("models/stt")));
    }

    // Stage 3: Build / Source-tree directory for un-installed Debug / IDE execution
    if (!appDir.isEmpty()) {
        // Walk up to 5 parent levels from the build binary directory (e.g. build/Desktop_Qt_Debug/Debug/)
        QDir cursor(appDir);
        for (int depth = 0; depth < 5; ++depth) {
            if (!cursor.cdUp()) {
                break;
            }
            const QString candidate = cursor.filePath(QStringLiteral("models/stt"));
            if (QFileInfo::exists(candidate)) {
                appendUniqueNormalizedDir(dirs, candidate);
            }
        }
    }
#ifdef QGC_STT_SOURCE_MODEL_DIR
    if (!isolateToOverrides) {
        appendUniqueNormalizedDir(dirs, QStringLiteral(QGC_STT_SOURCE_MODEL_DIR));
    }
#endif

    // Stage 4: OS application data directory (Linux ~/.local/share/..., Windows %LOCALAPPDATA%/...)
    if (!isolateToOverrides) {
        const QString appLocalData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!appLocalData.isEmpty()) {
            appendUniqueNormalizedDir(dirs, QDir(appLocalData).filePath(QStringLiteral("models/stt")));
        }
    }

    return dirs;
}

bool SttModelResolver::inspectModelFileOrDir(const QString& path, SttModelDescriptor& outDescriptor)
{
    const QFileInfo info(QDir::fromNativeSeparators(path.trimmed()));
    if (!info.exists()) {
        return false;
    }

    const QString fileName = info.fileName();
    const QString lowerName = fileName.toLower();
    const QString suffix = info.suffix().toLower();

    if (info.isFile()) {
        if (suffix != QStringLiteral("bin") && suffix != QStringLiteral("gguf") && suffix != QStringLiteral("onnx")) {
            return false;
        }

        // Skip empty 0-byte placeholder files
        if (info.size() <= 0) {
            return false;
        }
    } else if (info.isDir()) {
        const QDir dir(info.absoluteFilePath());
        const bool isVoskDir = dir.exists(QStringLiteral("am/final.mdl")) || dir.exists(QStringLiteral("final.mdl")) ||
                               lowerName.startsWith(QStringLiteral("vosk-model"));
        const bool isOnnxBundle = dir.exists(QStringLiteral("tokens.txt"));
        if (!isVoskDir && !isOnnxBundle) {
            return false;
        }
    } else {
        return false;
    }

    const SttEngineType engineType = SttEngineFactory::inferEngineTypeFromPath(info.absoluteFilePath());
    outDescriptor.id = fileName;
    outDescriptor.absoluteFilePath = info.absoluteFilePath();
    outDescriptor.engineType = engineType;
    outDescriptor.supportedLanguages = inferSupportedLanguagesFromName(lowerName);
    outDescriptor.displayName = QStringLiteral("%1 (%2)").arg(fileName, engineTypeLabel(engineType));
    return true;
}

QList<SttModelDescriptor> SttModelResolver::discoverModels(const QString& appDirPathOverride,
                                                           const QString& envDirOverride, bool isolateToOverrides)
{
    QList<SttModelDescriptor> discovered;
    QSet<QString> seenModelIds;

    // Scan each search directory in priority order
    const QStringList dirs = searchDirectories(appDirPathOverride, envDirOverride, isolateToOverrides);
    for (const QString& dirPath : dirs) {
        const QDir dir(dirPath);
        if (!dir.exists()) {
            continue;
        }

        const QFileInfoList entries =
            dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            SttModelDescriptor desc;
            if (!inspectModelFileOrDir(entry.absoluteFilePath(), desc)) {
                continue;
            }

            const QString key = desc.id.toLower();
            if (!seenModelIds.contains(key)) {
                seenModelIds.insert(key);
                discovered.append(desc);
            }
        }
    }

    return discovered;
}

bool SttModelResolver::resolveModel(const QString& selectedModelIdOrName, const QString& languageCode,
                                    SttModelDescriptor& outDescriptor,
                                    const QString& appDirPathOverride, const QString& envDirOverride,
                                    bool isolateToOverrides)
{
    const QList<SttModelDescriptor> models =
        discoverModels(appDirPathOverride, envDirOverride, isolateToOverrides);
    if (models.isEmpty()) {
        return false;
    }

    const QString requested = selectedModelIdOrName.trimmed();
    if (!requested.isEmpty()) {
        for (const SttModelDescriptor& desc : models) {
            if (desc.id.compare(requested, Qt::CaseInsensitive) == 0 ||
                desc.displayName.compare(requested, Qt::CaseInsensitive) == 0 ||
                desc.absoluteFilePath.compare(QDir::fromNativeSeparators(requested), Qt::CaseInsensitive) == 0) {
                outDescriptor = desc;
                return true;
            }
        }
    } else {
        // Fall back to preferred model supporting the requested language only when no model was explicitly requested
        for (const SttModelDescriptor& desc : models) {
            if (desc.supportedLanguages.contains(languageCode)) {
                outDescriptor = desc;
                return true;
            }
        }
    }

    // Otherwise fall back to the first discovered model on disk
    outDescriptor = models.first();
    return true;
}

SttConfig SttModelResolver::resolveConfig(int languageSettingIndex, const QString& selectedModelIdOrName,
                                          bool useGpu,
                                          const QString& appDirPathOverride, const QString& envDirOverride,
                                          bool isolateToOverrides)
{
    SttConfig cfg;
    cfg.language = languageCodeFromSettingIndex(languageSettingIndex);
    cfg.useGpu = useGpu;

    SttModelDescriptor resolvedDesc;
    if (resolveModel(selectedModelIdOrName, cfg.language, resolvedDesc, appDirPathOverride,
                     envDirOverride, isolateToOverrides)) {
        cfg.modelPath = resolvedDesc.absoluteFilePath;
    }

    return cfg;
}
