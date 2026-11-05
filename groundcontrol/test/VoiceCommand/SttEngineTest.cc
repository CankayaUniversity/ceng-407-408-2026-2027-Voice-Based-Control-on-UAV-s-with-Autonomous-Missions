#include "SttEngineTest.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtTest/QTest>

#include <cmath>
#include <cstdint>
#include <vector>

#include "SpeechToTextInterface.h"
#include "SttEngineFactory.h"
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

    // 1 second of 16kHz Int16 silence
    const QByteArray silencePcm(32000, '\0');
    const SttResult res = engine.transcribe(silencePcm);
    QVERIFY(res.inferenceTimeMs >= 0);

    engine.unload();
    QCOMPARE(engine.isLoaded(), false);
}

UT_REGISTER_TEST(SttEngineTest, TestLabel::Unit, TestLabel::Utilities)
