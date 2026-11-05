#include "SttEngineTest.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <cmath>
#include <cstdint>
#include <vector>

#include "SpeechToTextInterface.h"
#include "SttEngineFactory.h"
#include "SttModelManager.h"
#include "SttModelResolver.h"
#include "WhisperEngine.h"

void SttEngineTest::_testSttConfigResolvedThreadCount()
{
    SttConfig defaultConfig;
    const int autoThreads = defaultConfig.resolvedThreadCount();
    QVERIFY(autoThreads >= 2);
    QVERIFY(autoThreads <= 4);
    QVERIFY(defaultConfig.vocabularyHints.isEmpty());
    QCOMPARE(defaultConfig.useGpu, false);
    QCOMPARE(defaultConfig.language, QStringLiteral("en"));

    SttConfig explicitConfig;
    explicitConfig.threadCount = 6;
    QCOMPARE(explicitConfig.resolvedThreadCount(), 6);
}

void SttEngineTest::_testEngineFactoryInferType()
{
    QCOMPARE(SttEngineFactory::inferEngineTypeFromPath(QStringLiteral("")), SttEngineType::Whisper);
    QCOMPARE(SttEngineFactory::inferEngineTypeFromPath(QStringLiteral("/models/stt/ggml-base.en.bin")),
             SttEngineType::Whisper);
    QCOMPARE(SttEngineFactory::inferEngineTypeFromPath(QStringLiteral("C:/models/stt/ggml-tiny.bin")),
             SttEngineType::Whisper);
    QCOMPARE(SttEngineFactory::inferEngineTypeFromPath(QStringLiteral("/models/stt/sherpa-zipformer.onnx")),
             SttEngineType::Onnx);
    QCOMPARE(SttEngineFactory::inferEngineTypeFromPath(QStringLiteral("/models/stt/vosk-model-small-en-us-0.15")),
             SttEngineType::Vosk);
}

void SttEngineTest::_testEngineFactoryCreate()
{
    ignoreLogMessage("VoiceCommand.SttEngineFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Requested STT engine type .* is not yet registered")));

    auto whisperEngine = SttEngineFactory::createForModelPath(QStringLiteral("/models/stt/ggml-base.en.bin"));
    QVERIFY(whisperEngine != nullptr);
    QCOMPARE(whisperEngine->engineType(), SttEngineType::Whisper);
    QCOMPARE(whisperEngine->backendName(), QStringLiteral("whisper.cpp"));
    QCOMPARE(whisperEngine->isLoaded(), false);

    // Unimplemented Phase 2 engines return nullptr safely without crashing
    auto onnxEngine = SttEngineFactory::createForModelPath(QStringLiteral("/models/stt/model.onnx"));
    QVERIFY(onnxEngine == nullptr);
}

void SttEngineTest::_testDynamicModelDiscovery()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    const QDir dir(tempRoot.path());

    // 1. Empty directory must discover 0 models (no hardcoded models!)
    QList<SttModelDescriptor> models = SttModelResolver::discoverModels(QString(), QString(), dir.absolutePath(), true);
    QVERIFY(models.isEmpty());

    // 2. Drop a Whisper .bin file, an ONNX .onnx file, a Vosk model directory, and an invalid 0-byte file
    auto writeNonEmptyFile = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write("MODEL_DATA");
        f.close();
        return true;
    };

    QVERIFY(writeNonEmptyFile(dir.filePath(QStringLiteral("ggml-tiny.en.bin"))));
    QVERIFY(writeNonEmptyFile(dir.filePath(QStringLiteral("ggml-base.bin"))));
    QVERIFY(writeNonEmptyFile(dir.filePath(QStringLiteral("sherpa-zipformer.onnx"))));
    QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("vosk-model-small-en-us-0.15/am"))));
    QVERIFY(writeNonEmptyFile(dir.filePath(QStringLiteral("vosk-model-small-en-us-0.15/am/final.mdl"))));

    // Create an empty 0-byte .bin file and a unrelated .txt file — both should be ignored
    QFile emptyBin(dir.filePath(QStringLiteral("empty-broken.bin")));
    QVERIFY(emptyBin.open(QIODevice::WriteOnly));
    emptyBin.close();
    QVERIFY(writeNonEmptyFile(dir.filePath(QStringLiteral("readme.txt"))));

    models = SttModelResolver::discoverModels(QString(), QString(), dir.absolutePath(), true);
    QCOMPARE(models.size(), 4);

    SttModelDescriptor tinyEn;
    QVERIFY(SttModelResolver::inspectModelFileOrDir(dir.filePath(QStringLiteral("ggml-tiny.en.bin")), tinyEn));
    QCOMPARE(tinyEn.id, QStringLiteral("ggml-tiny.en.bin"));
    QCOMPARE(tinyEn.engineType, SttEngineType::Whisper);
    QVERIFY(tinyEn.supportedLanguages.contains(QStringLiteral("en")));
    QVERIFY(!tinyEn.supportedLanguages.contains(QStringLiteral("tr")));

    SttModelDescriptor baseMulti;
    QVERIFY(SttModelResolver::inspectModelFileOrDir(dir.filePath(QStringLiteral("ggml-base.bin")), baseMulti));
    QCOMPARE(baseMulti.id, QStringLiteral("ggml-base.bin"));
    QCOMPARE(baseMulti.engineType, SttEngineType::Whisper);
    QVERIFY(baseMulti.supportedLanguages.contains(QStringLiteral("en")));
    QVERIFY(baseMulti.supportedLanguages.contains(QStringLiteral("tr")));

    QCOMPARE(SttModelResolver::languageCodeFromSettingIndex(0), QStringLiteral("en"));
    QCOMPARE(SttModelResolver::languageCodeFromSettingIndex(1), QStringLiteral("tr"));
}

void SttEngineTest::_testModelResolverFiveStageCascade()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    const QDir rootDir(tempRoot.path());

    // Create simulated directories for Stage 1 (custom), Stage 2 (env), Stage 3 (appDir/models/stt),
    // and Stage 4 (parent source tree models/stt via upward traversal from fake build/Debug/ bin dir).
    const QString customFilePath = rootDir.filePath(QStringLiteral("custom/my-custom-whisper.bin"));
    const QString envSttDir = rootDir.filePath(QStringLiteral("env_models"));
    const QString releaseAppDir = rootDir.filePath(QStringLiteral("release_bundle"));
    const QString releaseSttDir = QDir(releaseAppDir).filePath(QStringLiteral("models/stt"));
    const QString debugSourceRoot = rootDir.filePath(QStringLiteral("workspace_root"));
    const QString debugSourceSttDir = QDir(debugSourceRoot).filePath(QStringLiteral("models/stt"));
    const QString debugBinaryDir = QDir(debugSourceRoot).filePath(QStringLiteral("build/Desktop_Debug/Debug"));

    QVERIFY(QDir().mkpath(QFileInfo(customFilePath).absolutePath()));
    QVERIFY(QDir().mkpath(envSttDir));
    QVERIFY(QDir().mkpath(releaseSttDir));
    QVERIFY(QDir().mkpath(debugSourceSttDir));
    QVERIFY(QDir().mkpath(debugBinaryDir));

    auto touchFile = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write("GGML");
        f.close();
        return true;
    };

    const QString debugModelFile = QDir(debugSourceSttDir).filePath(QStringLiteral("ggml-base.en.bin"));
    const QString releaseModelFile = QDir(releaseSttDir).filePath(QStringLiteral("ggml-base.en.bin"));
    const QString envModelFile = QDir(envSttDir).filePath(QStringLiteral("ggml-base.en.bin"));

    SttModelDescriptor desc;

    // 1. Only Stage 4 (upward traversal from Debug binary dir to workspace_root/models/stt) exists
    QVERIFY(touchFile(debugModelFile));
    QVERIFY(SttModelResolver::resolveModel(QStringLiteral("ggml-base.en.bin"), QStringLiteral("en"), desc, QString(),
                                           debugBinaryDir, QString(), true));
    QCOMPARE(desc.absoluteFilePath, QFileInfo(debugModelFile).absoluteFilePath());

    // 2. Stage 3 (executable-relative models/stt) takes priority over Stage 4
    QVERIFY(touchFile(releaseModelFile));
    QVERIFY(SttModelResolver::resolveModel(QStringLiteral("ggml-base.en.bin"), QStringLiteral("en"), desc, QString(),
                                           releaseAppDir, QString(), true));
    QCOMPARE(desc.absoluteFilePath, QFileInfo(releaseModelFile).absoluteFilePath());

    // 3. Stage 2 (QGC_STT_MODEL_DIR env override) takes priority over Stage 3 and Stage 4
    QVERIFY(touchFile(envModelFile));
    QVERIFY(SttModelResolver::resolveModel(QStringLiteral("ggml-base.en.bin"), QStringLiteral("en"), desc, QString(),
                                           releaseAppDir, envSttDir, true));
    QCOMPARE(desc.absoluteFilePath, QFileInfo(envModelFile).absoluteFilePath());

    // 4. Stage 1 (Custom model file path) takes highest priority over all other stages
    QVERIFY(touchFile(customFilePath));
    QVERIFY(SttModelResolver::resolveModel(QStringLiteral("ggml-base.en.bin"), QStringLiteral("en"), desc,
                                           customFilePath, releaseAppDir, envSttDir, true));
    QCOMPARE(desc.absoluteFilePath, QFileInfo(customFilePath).absoluteFilePath());
}

void SttEngineTest::_testResolveConfigTurkishUpgrade()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    const QDir envDir(tempRoot.path());
    const QString enOnlyFile = envDir.filePath(QStringLiteral("ggml-base.en.bin"));
    const QString multiFile = envDir.filePath(QStringLiteral("ggml-base.bin"));
    const QString customUniversal = envDir.filePath(QStringLiteral("whisper-large-v3-turbo.bin"));

    auto writeDummy = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write("GGML");
        f.close();
        return true;
    };

    QVERIFY(writeDummy(enOnlyFile));
    QVERIFY(writeDummy(multiFile));
    QVERIFY(writeDummy(customUniversal));

    // 1. When no model is explicitly requested (empty), Turkish language selection
    // falls back to the first discovered model supporting Turkish ("ggml-base.bin").
    const SttConfig cfgFallback = SttModelResolver::resolveConfig(
        1 /* Turkish */, QString(), QString(), true /* useGpu */, QString(), envDir.absolutePath(), true);
    QCOMPARE(cfgFallback.language, QStringLiteral("tr"));
    QCOMPARE(cfgFallback.useGpu, true);
    QCOMPARE(cfgFallback.modelPath, QFileInfo(multiFile).absoluteFilePath());

    // 2. When the user explicitly selects a model by name/ID, that selection MUST be honored
    // even if language is Turkish and the model does not have explicit 'tr' in its name.
    const SttConfig cfgExplicitUniversal = SttModelResolver::resolveConfig(
        1 /* Turkish */, QStringLiteral("whisper-large-v3-turbo.bin"), QString(), false, QString(),
        envDir.absolutePath(), true);
    QCOMPARE(cfgExplicitUniversal.modelPath, QFileInfo(customUniversal).absoluteFilePath());

    // 3. Explicitly selecting an English-only model with Turkish language is still honored.
    const SttConfig cfgExplicitEn = SttModelResolver::resolveConfig(
        1 /* Turkish */, QStringLiteral("ggml-base.en.bin"), QString(), false, QString(),
        envDir.absolutePath(), true);
    QCOMPARE(cfgExplicitEn.modelPath, QFileInfo(enOnlyFile).absoluteFilePath());
}

void SttEngineTest::_testInspectModelFileOrDirBoundary()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    const QDir dir(tempRoot.path());

    auto writeDummy = [](const QString& path, const QByteArray& data = "DATA") {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write(data);
        f.close();
        return true;
    };

    SttModelDescriptor desc;

    // 1. Non-existent path returns false
    QVERIFY(!SttModelResolver::inspectModelFileOrDir(dir.filePath(QStringLiteral("nonexistent.bin")), desc));

    // 2. Empty 0-byte file returns false
    const QString emptyFile = dir.filePath(QStringLiteral("empty.bin"));
    QVERIFY(writeDummy(emptyFile, QByteArray()));
    QVERIFY(!SttModelResolver::inspectModelFileOrDir(emptyFile, desc));

    // 3. Uppercase file extensions (.BIN, .GGUF, .ONNX) must be accepted
    const QString upperBin = dir.filePath(QStringLiteral("MODEL_UPPER.BIN"));
    QVERIFY(writeDummy(upperBin));
    QVERIFY(SttModelResolver::inspectModelFileOrDir(upperBin, desc));
    QCOMPARE(desc.id, QStringLiteral("MODEL_UPPER.BIN"));
    QCOMPARE(desc.engineType, SttEngineType::Whisper);

    const QString upperGguf = dir.filePath(QStringLiteral("WHISPER_TINY.GGUF"));
    QVERIFY(writeDummy(upperGguf));
    QVERIFY(SttModelResolver::inspectModelFileOrDir(upperGguf, desc));
    QCOMPARE(desc.engineType, SttEngineType::Whisper);

    const QString upperOnnx = dir.filePath(QStringLiteral("ENCODER.ONNX"));
    QVERIFY(writeDummy(upperOnnx));
    QVERIFY(SttModelResolver::inspectModelFileOrDir(upperOnnx, desc));
    QCOMPARE(desc.engineType, SttEngineType::Onnx);

    // 4. Unsupported extensions return false
    const QString textFile = dir.filePath(QStringLiteral("model.txt"));
    QVERIFY(writeDummy(textFile));
    QVERIFY(!SttModelResolver::inspectModelFileOrDir(textFile, desc));

    // 5. Vosk directory with am/final.mdl
    const QString voskDir1 = dir.filePath(QStringLiteral("vosk-model-en"));
    QVERIFY(QDir().mkpath(QDir(voskDir1).filePath(QStringLiteral("am"))));
    QVERIFY(writeDummy(QDir(voskDir1).filePath(QStringLiteral("am/final.mdl"))));
    QVERIFY(SttModelResolver::inspectModelFileOrDir(voskDir1, desc));
    QCOMPARE(desc.engineType, SttEngineType::Vosk);

    const QString voskDir2 = dir.filePath(QStringLiteral("vosk-model-turkish"));
    QVERIFY(QDir().mkpath(voskDir2));
    QVERIFY(SttModelResolver::inspectModelFileOrDir(voskDir2, desc));
    QCOMPARE(desc.engineType, SttEngineType::Vosk);
    QVERIFY(desc.supportedLanguages.contains(QStringLiteral("tr")));

    // 6. ONNX bundle directory with tokens.txt
    const QString onnxBundleDir = dir.filePath(QStringLiteral("sherpa_bundle"));
    QVERIFY(QDir().mkpath(onnxBundleDir));
    QVERIFY(writeDummy(QDir(onnxBundleDir).filePath(QStringLiteral("tokens.txt"))));
    QVERIFY(SttModelResolver::inspectModelFileOrDir(onnxBundleDir, desc));
    QCOMPARE(desc.engineType, SttEngineType::Onnx);

    // 7. Empty arbitrary directory returns false
    const QString emptyDir = dir.filePath(QStringLiteral("random_dir"));
    QVERIFY(QDir().mkpath(emptyDir));
    QVERIFY(!SttModelResolver::inspectModelFileOrDir(emptyDir, desc));
}

void SttEngineTest::_testDiscoverModelsPrecedenceAndShadowing()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    const QDir root(tempRoot.path());

    const QString stage1CustomDir = root.filePath(QStringLiteral("stage1_custom"));
    const QString stage2EnvDir = root.filePath(QStringLiteral("stage2_env"));
    const QString stage3AppDir = root.filePath(QStringLiteral("stage3_app"));
    const QString stage3ModelsDir = QDir(stage3AppDir).filePath(QStringLiteral("models/stt"));

    QVERIFY(QDir().mkpath(stage1CustomDir));
    QVERIFY(QDir().mkpath(stage2EnvDir));
    QVERIFY(QDir().mkpath(stage3ModelsDir));

    auto writeDummy = [](const QString& path, const QByteArray& data) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write(data);
        f.close();
        return true;
    };

    // Create a duplicate model name in both Stage 2 (env) and Stage 3 (app):
    // Stage 2 has priority and must shadow Stage 3.
    const QString envDupe = QDir(stage2EnvDir).filePath(QStringLiteral("shadow-model.bin"));
    const QString appDupe = QDir(stage3ModelsDir).filePath(QStringLiteral("shadow-model.bin"));
    const QString appUnique = QDir(stage3ModelsDir).filePath(QStringLiteral("app-unique.bin"));

    QVERIFY(writeDummy(envDupe, "ENV_VERSION"));
    QVERIFY(writeDummy(appDupe, "APP_VERSION"));
    QVERIFY(writeDummy(appUnique, "APP_UNIQUE"));

    // Discover with env override and app dir override
    QList<SttModelDescriptor> discovered = SttModelResolver::discoverModels(
        QString(), stage3AppDir, stage2EnvDir, true /* isolate */);

    // Should discover 2 models total: "shadow-model.bin" (from env) and "app-unique.bin" (from app)
    QCOMPARE(discovered.size(), 2);
    QCOMPARE(discovered[0].id, QStringLiteral("shadow-model.bin"));
    QCOMPARE(discovered[0].absoluteFilePath, QFileInfo(envDupe).absoluteFilePath());
    QCOMPARE(discovered[1].id, QStringLiteral("app-unique.bin"));

    // Custom directory (Stage 1) priority over Stage 2:
    const QString customDupe = QDir(stage1CustomDir).filePath(QStringLiteral("shadow-model.bin"));
    QVERIFY(writeDummy(customDupe, "CUSTOM_VERSION"));

    discovered = SttModelResolver::discoverModels(
        stage1CustomDir, stage3AppDir, stage2EnvDir, true /* isolate */);
    QCOMPARE(discovered.size(), 2);
    QCOMPARE(discovered[0].id, QStringLiteral("shadow-model.bin"));
    QCOMPARE(discovered[0].absoluteFilePath, QFileInfo(customDupe).absoluteFilePath());

    // Custom direct file (Stage 1 file) takes highest priority and appears first
    const QString directCustomFile = root.filePath(QStringLiteral("isolated-special.bin"));
    QVERIFY(writeDummy(directCustomFile, "SPECIAL"));

    discovered = SttModelResolver::discoverModels(
        directCustomFile, stage3AppDir, stage2EnvDir, true /* isolate */);
    QCOMPARE(discovered.first().id, QStringLiteral("isolated-special.bin"));
    QCOMPARE(discovered.first().absoluteFilePath, QFileInfo(directCustomFile).absoluteFilePath());
}

void SttEngineTest::_testSttModelManager()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());
    const QDir root(tempRoot.path());

    const QString modelDir = root.filePath(QStringLiteral("stt_models"));
    QVERIFY(QDir().mkpath(modelDir));

    SttModelManager manager;
    manager.setSearchOverride(modelDir, true);

    QCOMPARE(manager.isModelAvailable(), false);
    QCOMPARE(manager.availableModels().size(), 0);
    QCOMPARE(manager.selectedModelIndex(), -1);

    auto writeDummy = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        f.write("GGML");
        f.close();
        return true;
    };

    const QString modelA = QDir(modelDir).filePath(QStringLiteral("model-a.bin"));
    const QString modelB = QDir(modelDir).filePath(QStringLiteral("model-b.bin"));
    QVERIFY(writeDummy(modelA));
    QVERIFY(writeDummy(modelB));

    QSignalSpy availableSpy(&manager, &SttModelManager::isModelAvailableChanged);
    QSignalSpy modelsSpy(&manager, &SttModelManager::availableModelsChanged);

    manager.refreshModels();

    QCOMPARE(manager.isModelAvailable(), true);
    QCOMPARE(manager.availableModels().size(), 2);
    QCOMPARE(availableSpy.count(), 1);
    QCOMPARE(modelsSpy.count(), 1);

    // Default selected model should be the first discovered model
    QCOMPARE(manager.selectedModelIndex(), 0);
    QCOMPARE(manager.selectedModel(), QStringLiteral("model-a.bin"));

    // Changing selection by index
    manager.setSelectedModelIndex(1);
    QCOMPARE(manager.selectedModelIndex(), 1);
    QCOMPARE(manager.selectedModel(), QStringLiteral("model-b.bin"));

    // Invalid index should be ignored safely
    manager.setSelectedModelIndex(99);
    QCOMPARE(manager.selectedModelIndex(), 1);
    manager.setSelectedModelIndex(-1);
    QCOMPARE(manager.selectedModelIndex(), 1);

    // Changing selection by ID
    manager.setSelectedModel(QStringLiteral("model-a.bin"));
    QCOMPARE(manager.selectedModelIndex(), 0);
    QCOMPARE(manager.selectedModel(), QStringLiteral("model-a.bin"));

    // Resolving config through manager
    const SttConfig cfg = manager.currentResolvedConfig(0 /* English */, false /* useGpu */);
    QCOMPARE(cfg.modelPath, QFileInfo(modelA).absoluteFilePath());
}

void SttEngineTest::_testPcm16ToFloatConversion()
{
    std::vector<float> samples;
    QString errorMsg;

    // 1. Empty buffer must fail
    QCOMPARE(WhisperEngine::convertPcm16ToFloat(QByteArray(), samples, &errorMsg), false);
    QVERIFY(!errorMsg.isEmpty());

    // 2. Odd-byte misaligned buffer must fail
    errorMsg.clear();
    QCOMPARE(WhisperEngine::convertPcm16ToFloat(QByteArray(3, '\0'), samples, &errorMsg), false);
    QVERIFY(!errorMsg.isEmpty());

    // 3. Valid 16-bit PCM samples: 0, +16384 (~0.5f), -32768 (-1.0f), +32767 (~1.0f)
    const int16_t rawValues[4] = {0, 16384, -32768, 32767};
    const QByteArray validPcm(reinterpret_cast<const char*>(rawValues), sizeof(rawValues));

    errorMsg.clear();
    QVERIFY(WhisperEngine::convertPcm16ToFloat(validPcm, samples, &errorMsg));
    QCOMPARE(samples.size(), size_t(4));
    QVERIFY(std::fabs(samples[0] - 0.0f) < 1e-6f);
    QVERIFY(std::fabs(samples[1] - 0.5f) < 1e-6f);
    QVERIFY(std::fabs(samples[2] - (-1.0f)) < 1e-6f);
    QVERIFY(samples[3] > 0.999f && samples[3] <= 1.0f);
}

void SttEngineTest::_testWhisperEngineLifecycleAndErrorHandling()
{
    ignoreLogMessage("VoiceCommand.WhisperEngine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WhisperEngine is not initialized|Cannot initialize WhisperEngine")));

    WhisperEngine engine;
    QCOMPARE(engine.isLoaded(), false);

    // Transcribing before initialization must fail gracefully
    const QByteArray dummyPcm(32000, '\0');  // 1 second of 16 kHz Mono Int16 silence
    const SttResult uninitResult = engine.transcribe(dummyPcm);
    QCOMPARE(uninitResult.success, false);
    QVERIFY(!uninitResult.errorMessage.isEmpty());

    // Initializing with empty model path must fail gracefully
    SttConfig emptyConfig;
    QCOMPARE(engine.initialize(emptyConfig), false);
    QCOMPARE(engine.isLoaded(), false);

    // Initializing with non-existent model file must fail gracefully
    SttConfig missingConfig;
    missingConfig.modelPath = QStringLiteral("/nonexistent/path/ggml-missing-model.bin");
    QCOMPARE(engine.initialize(missingConfig), false);
    QCOMPARE(engine.isLoaded(), false);

    // Calling unload() when not loaded must be a safe no-op
    engine.unload();
    QCOMPARE(engine.isLoaded(), false);
}

void SttEngineTest::_testWhisperTranscriptionIfModelAvailable()
{
    const QString envModelDir = qEnvironmentVariable("QGC_STT_MODEL_DIR");
    QStringList candidatePaths;
    if (!envModelDir.isEmpty()) {
        candidatePaths << QDir(envModelDir).filePath(QStringLiteral("ggml-tiny.en.bin"))
                       << QDir(envModelDir).filePath(QStringLiteral("ggml-base.en.bin"));
    }

    QString foundModelPath;
    for (const QString& path : candidatePaths) {
        if (QFileInfo::exists(path)) {
            foundModelPath = path;
            break;
        }
    }

    if (foundModelPath.isEmpty()) {
        // No local model binary configured in environment; lifecycle tests already covered engine contract.
        return;
    }

    WhisperEngine engine;
    SttConfig cfg;
    cfg.modelPath = foundModelPath;
    cfg.language = QStringLiteral("en");
    cfg.useGpu = false;

    QVERIFY(engine.initialize(cfg));
    QVERIFY(engine.isLoaded());

    // Verify context reuse and unconditional config update (e.g. thread count / language)
    SttConfig updatedCfg = cfg;
    updatedCfg.language = QStringLiteral("tr");
    updatedCfg.threadCount = 4;
    QVERIFY(engine.initialize(updatedCfg));
    QVERIFY(engine.isLoaded());
    QCOMPARE(engine.config().language, QStringLiteral("tr"));
    QCOMPARE(engine.config().threadCount, 4);

    // 1 second of 16kHz Int16 silence
    const QByteArray silencePcm(32000, '\0');
    const SttResult res = engine.transcribe(silencePcm);
    QVERIFY(res.inferenceTimeMs >= 0);

    engine.unload();
    QCOMPARE(engine.isLoaded(), false);
}

UT_REGISTER_TEST(SttEngineTest, TestLabel::Unit, TestLabel::Utilities)
