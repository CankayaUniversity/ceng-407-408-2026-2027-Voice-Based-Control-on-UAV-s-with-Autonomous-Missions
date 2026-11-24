#include "VoiceCommandManagerTest.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QKeyEvent>
#include <QtTest/QSignalSpy>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "IntentParserInterface.h"
#include "SpeechToTextInterface.h"
#include "VoiceCommandManager.h"

namespace {

class MockSttEngine : public SpeechToTextInterface
{
public:
    bool initialize(const SttConfig& config) override
    {
        _config = config;
        _loaded = true;
        _initCount++;
        return true;
    }

    void unload() override
    {
        _loaded = false;
        _unloadCount++;
    }

    bool isLoaded() const override { return _loaded; }

    SttConfig config() const override { return _config; }

    SttResult transcribe(const QByteArray& pcm16kMono) override
    {
        Q_UNUSED(pcm16kMono);
        _transcribeCount++;
        return _mockResult;
    }

    QString backendName() const override { return QStringLiteral("mock"); }

    SttEngineType engineType() const override { return SttEngineType::Custom; }

    bool _loaded = false;
    int _initCount = 0;
    int _unloadCount = 0;
    int _transcribeCount = 0;
    SttConfig _config;
    SttResult _mockResult{true, QStringLiteral("arm the drone"), 0.95f, 10, QString()};
};

class RejectingParser : public IntentParserInterface
{
public:
    IntentParseResult parse(const QString& transcript, const VehicleStateSnapshot&) override
    {
        return {false, transcript, {}, QStringLiteral("Unrecognized command")};
    }
};

}  // namespace

void VoiceCommandManagerTest::init()
{
    UnitTest::init();
    ignoreLogMessage("qt.multimedia.pipewire.registry", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to connect to pipewire")));
    ignoreLogMessage("default", QtWarningMsg, QRegularExpression(QStringLiteral("PulseAudioService|secure directory")));
    ignoreLogMessage("Utilities.AudioCapture", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Cannot start capture: No valid active microphone\\.")));
    VoiceCommandManager::instance()->init(nullptr);
    VoiceCommandManager::instance()->setPushToTalkKey(QStringLiteral("P"));
    VoiceCommandManager::instance()->setVehicleCheckEnabled(false);
    VoiceCommandManager::instance()->setSttModelCheckEnabled(false);
    VoiceCommandManager::instance()->setSttEngine(nullptr, false);
    VoiceCommandManager::instance()->setIntentParser(nullptr);
    VoiceCommandManager::instance()->resetSession();
}

void VoiceCommandManagerTest::cleanup()
{
    VoiceCommandManager::instance()->stopRecognitionTest();
    if (VoiceCommandManager::instance()->isRecording()) {
        VoiceCommandManager::instance()->stopRecording();
    }
    VoiceCommandManager::instance()->resetSession();
    VoiceCommandManager::instance()->setSttEngine(nullptr, false);
    VoiceCommandManager::instance()->setIntentParser(nullptr);
    if (VoiceCommandManager::instance()->sttModelManager()) {
        VoiceCommandManager::instance()->sttModelManager()->setSearchOverride(QString(), false);
    }
    VoiceCommandManager::instance()->setSttModelCheckEnabled(false);
    UnitTest::cleanup();
}

void VoiceCommandManagerTest::_testInitialState()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QVERIFY(manager != nullptr);
    QCOMPARE(manager->isRecording(), false);
    QCOMPARE(manager->pushToTalkKey(), QStringLiteral("P"));
    QCOMPARE(manager->isMicrophoneActive(), AudioDeviceManager::instance()->isMicrophoneActive());
}

void VoiceCommandManagerTest::_testPushToTalkKeyConfiguration()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QSignalSpy spy(manager, &VoiceCommandManager::pushToTalkKeyChanged);

    manager->setPushToTalkKey(QStringLiteral("V"));
    QCOMPARE(manager->pushToTalkKey(), QStringLiteral("V"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("V"));

    // Setting same key should not re-emit
    manager->setPushToTalkKey(QStringLiteral("V"));
    QCOMPARE(spy.count(), 1);

    // Setting empty key should be ignored
    manager->setPushToTalkKey(QStringLiteral(""));
    QCOMPARE(manager->pushToTalkKey(), QStringLiteral("V"));
}

void VoiceCommandManagerTest::_testRecordingLifecycleDelegation()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    AudioCapture* capture = AudioCapture::instance();

    QSignalSpy spyRecordingChanged(manager, &VoiceCommandManager::isRecordingChanged);

    if (manager->isMicrophoneActive()) {
        const bool started = manager->startRecording();
        QVERIFY(started);
        QVERIFY(manager->isRecording());
        QVERIFY(capture->isRecording());

        manager->stopRecording();
        QCOMPARE(manager->isRecording(), false);
        QCOMPARE(capture->isRecording(), false);
    } else {
        // Without active microphone, recording cannot start
        const bool started = manager->startRecording();
        QCOMPARE(started, false);
        QCOMPARE(manager->isRecording(), false);
    }
}

void VoiceCommandManagerTest::_testSignalPropagation()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    manager->setSttEngine(new MockSttEngine(), true);
    QSignalSpy spyRecorded(manager, &VoiceCommandManager::voiceCommandRecorded);

    // Canonical 16kHz Int16 Mono requires >=16000 bytes for >=500ms
    const QByteArray testPcm(16000, '\0');
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    manager->_onRecordingFinished(testPcm, format);

    QCOMPARE(spyRecorded.count(), 1);
    QCOMPARE(spyRecorded.first().at(0).toByteArray(), testPcm);
}

void VoiceCommandManagerTest::_testKeyEventFilter()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    manager->setPushToTalkKey(QStringLiteral("P"));

    // Create a key press event for 'P'
    QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, QStringLiteral("p"));
    const bool handledPress = manager->eventFilter(nullptr, &pressEvent);

    QVERIFY(handledPress);
    QVERIFY(manager->_isKeyPressed);

    // Create a key release event for 'P'
    QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_P, Qt::NoModifier, QStringLiteral("p"));
    const bool handledRelease = manager->eventFilter(nullptr, &releaseEvent);

    QVERIFY(handledRelease);
    QCOMPARE(manager->_isKeyPressed, false);
}

void VoiceCommandManagerTest::_testAutoRepeatIgnored()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    manager->setPushToTalkKey(QStringLiteral("P"));

    // Auto-repeat press should be ignored
    QKeyEvent autoRepeatPress(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, QStringLiteral("p"), true);
    const bool handledAutoPress = manager->eventFilter(nullptr, &autoRepeatPress);
    QCOMPARE(handledAutoPress, false);

    // Auto-repeat release should be ignored
    QKeyEvent autoRepeatRelease(QEvent::KeyRelease, Qt::Key_P, Qt::NoModifier, QStringLiteral("p"), true);
    const bool handledAutoRelease = manager->eventFilter(nullptr, &autoRepeatRelease);
    QCOMPARE(handledAutoRelease, false);
}

void VoiceCommandManagerTest::_testVehicleConnectedGating()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    manager->setVehicleCheckEnabled(true);

    // In unit test environment without an active vehicle, vehicle connection must be false
    QCOMPARE(manager->isVehicleConnected(), false);

    // When no vehicle is connected, startRecording must immediately fail
    const bool started = manager->startRecording();
    QCOMPARE(started, false);
    QCOMPARE(manager->isRecording(), false);

    // Keyboard press must also be ignored when no vehicle is connected
    QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, QStringLiteral("p"));
    const bool handledPress = manager->eventFilter(nullptr, &pressEvent);
    QCOMPARE(handledPress, false);

    // Restore vehicle check disabled for test harness
    manager->setVehicleCheckEnabled(false);
    QCOMPARE(manager->isVehicleConnected(), true);
}

void VoiceCommandManagerTest::_testSttModelAvailableGating()
{
    ignoreLogMessage("VoiceCommand.VoiceCommandManager", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Model for voice recognition is not found")));

    VoiceCommandManager* manager = VoiceCommandManager::instance();
    SttModelManager* modelManager = manager->sttModelManager();
    QVERIFY(modelManager != nullptr);

    QTemporaryDir emptyModelDir;
    QVERIFY(emptyModelDir.isValid());

    manager->setSttModelCheckEnabled(true);
    modelManager->setSearchOverride(emptyModelDir.path(), true);

    // 1. When no model files exist on disk, SttModelManager has 0 models and VoiceCommandManager is not ready
    QCOMPARE(modelManager->isModelAvailable(), false);
    QCOMPARE(manager->isSttModelAvailable(), false);
    QCOMPARE(manager->isReady(), false);
    QVERIFY(modelManager->availableModels().isEmpty());
    QCOMPARE(modelManager->selectedModelIndex(), -1);

    QSignalSpy warningSpy(manager, &VoiceCommandManager::voiceCommandWarning);

    // Calling startRecording (e.g. pressing disabled PTT button) must emit "Model for voice recognition is not found"
    const bool started = manager->startRecording();
    QCOMPARE(started, false);
    QCOMPARE(manager->isRecording(), false);
    QCOMPARE(warningSpy.count(), 1);
    QCOMPARE(warningSpy.first().first().toString(), QStringLiteral("Model for voice recognition is not found"));

    // Pressing PTT shortcut key when no model is found must also emit the warning and not start recording
    QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, QStringLiteral("p"));
    const bool handledPress = manager->eventFilter(nullptr, &pressEvent);
    QCOMPARE(handledPress, true);
    QCOMPARE(manager->isRecording(), false);
    QCOMPARE(warningSpy.count(), 2);
    QCOMPARE(warningSpy.last().first().toString(), QStringLiteral("Model for voice recognition is not found"));

    // 2. Dropping a valid .bin model into the directory and calling modelManager->refreshModels() makes it ready
    const QString modelFile = QDir(emptyModelDir.path()).filePath(QStringLiteral("ggml-base.en.bin"));
    QFile f(modelFile);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("GGML_MODEL");
    f.close();

    modelManager->refreshModels();
    QCOMPARE(modelManager->isModelAvailable(), true);
    QCOMPARE(manager->isSttModelAvailable(), true);
    QCOMPARE(modelManager->availableModels().size(), 1);
    QCOMPARE(modelManager->selectedModel(), QStringLiteral("ggml-base.en.bin"));
    QCOMPARE(modelManager->selectedModelIndex(), 0);
}

void VoiceCommandManagerTest::_testMinimumDurationValidation()
{
    ignoreLogMessage("VoiceCommand.VoiceCommandManager", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Voice Command is too short to recognize")));

    VoiceCommandManager* manager = VoiceCommandManager::instance();
    manager->setSttEngine(new MockSttEngine(), true);
    QSignalSpy spyWarning(manager, &VoiceCommandManager::voiceCommandWarning);
    QSignalSpy spyRecorded(manager, &VoiceCommandManager::voiceCommandRecorded);

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    // 1. Audio shorter than 500ms (<16000 bytes at 16kHz Int16 Mono) must be rejected
    const QByteArray shortPcm(8000, '\0');  // 250ms
    manager->_onRecordingFinished(shortPcm, format);

    QCOMPARE(spyRecorded.count(), 0);
    QCOMPARE(spyWarning.count(), 1);
    QCOMPARE(spyWarning.first().first().toString(), QStringLiteral("Voice Command is too short to recognize"));
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);

    // 2. Audio >= 500ms (>=16000 bytes) must pass through
    const QByteArray validPcm(16000, '\0');  // exactly 500ms
    manager->_onRecordingFinished(validPcm, format);

    QCOMPARE(spyRecorded.count(), 1);
    QCOMPARE(spyWarning.count(), 1);  // no new warning emitted
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Processing);
}

void VoiceCommandManagerTest::_testSessionStateMachine()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    QCOMPARE(manager->sessionStateString(), QStringLiteral("Idle"));

    QSignalSpy spyState(manager, &VoiceCommandManager::sessionStateChanged);

    if (manager->isMicrophoneActive()) {
        manager->startRecording();
        QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Listening);
        QCOMPARE(manager->sessionStateString(), QStringLiteral("Listening"));
        QVERIFY(spyState.count() >= 1);

        manager->resetSession();
        QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    }
}

void VoiceCommandManagerTest::_testPreWarmingOnPttPress()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QTemporaryDir modelDir;
    QVERIFY(modelDir.isValid());
    QFile model(QDir(modelDir.path()).filePath(QStringLiteral("ggml-base.en.bin")));
    QVERIFY(model.open(QIODevice::WriteOnly));
    QVERIFY(model.write("GGML_MODEL") > 0);
    model.close();
    manager->sttModelManager()->setSearchOverride(modelDir.path(), true);

    auto* mockEngine = new MockSttEngine();
    mockEngine->_loaded = false;
    manager->setSttEngine(mockEngine, true);

    QSignalSpy spyLoaded(manager, &VoiceCommandManager::modelLoadedChanged);

    if (manager->isMicrophoneActive()) {
        manager->startRecording();
        QVERIFY(spyLoaded.wait(1000) || manager->isSttModelLoaded());
        manager->stopRecording();
    }
}

void VoiceCommandManagerTest::_testIdleUnloadTimer()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    auto* mockEngine = new MockSttEngine();
    mockEngine->_loaded = true;
    manager->setSttEngine(mockEngine, true);
    QVERIFY(manager->isSttModelLoaded());

    manager->setIdleUnloadTimeout(50);  // 50ms
    manager->setKeepModelInMemory(false);

    QSignalSpy spyLoaded(manager, &VoiceCommandManager::modelLoadedChanged);

    SttResult res;
    res.success = true;
    res.text = QStringLiteral("land");
    manager->_onTranscriptionFinished(res);

    QVERIFY(spyLoaded.wait(1000));
    QCOMPARE(manager->isSttModelLoaded(), false);

    manager->resetSession();
    mockEngine->_loaded = true;
    manager->_onModelInitialized(true);
    QVERIFY(manager->isSttModelLoaded());
    QVERIFY(spyLoaded.wait(1000));
    QCOMPARE(manager->isSttModelLoaded(), false);

    mockEngine->_loaded = true;
    manager->_onModelInitialized(true);
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    manager->_onRecordingFinished(QByteArray(8000, '\0'), format);
    QVERIFY(spyLoaded.wait(1000));
    QCOMPARE(manager->isSttModelLoaded(), false);
}

void VoiceCommandManagerTest::_testAsynchronousWorkerTranscription()
{
    ignoreLogMessage("VoiceCommand.VoiceCommandManager", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Voice Command is Failed|STT transcription failed:.*")));

    VoiceCommandManager* manager = VoiceCommandManager::instance();
    auto* mockEngine = new MockSttEngine();
    mockEngine->_mockResult = SttResult{true, QStringLiteral("take off"), 0.98f, 20, QString()};
    manager->setSttEngine(mockEngine, true);

    QSignalSpy spyTranscribed(manager, &VoiceCommandManager::voiceCommandTranscribed);
    QSignalSpy spyConfirmation(manager, &VoiceCommandManager::commandConfirmationRequested);

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const QByteArray validPcm(16000, '\0');  // 500ms
    manager->_onRecordingFinished(validPcm, format);

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Processing);

    QVERIFY(spyTranscribed.wait(1000));
    QCOMPARE(spyTranscribed.count(), 1);
    QCOMPARE(spyTranscribed.first().at(0).toString(), QStringLiteral("take off"));
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Confirming);
    QCOMPARE(spyConfirmation.count(), 1);
    const auto parsed = spyConfirmation.first().first().value<IntentParseResult>();
    QCOMPARE(parsed.intents.size(), 1);
    QCOMPARE(parsed.intents.first().action, VoiceCommandAction::Takeoff);

    // Test failure scenario
    auto* failEngine = new MockSttEngine();
    failEngine->_mockResult = SttResult{false, QString(), 0.0f, 10, QStringLiteral("Inference error")};
    manager->setSttEngine(failEngine, true);

    QSignalSpy spyWarning(manager, &VoiceCommandManager::voiceCommandWarning);
    QSignalSpy spyState(manager, &VoiceCommandManager::sessionStateChanged);
    manager->_onRecordingFinished(validPcm, format);

    QVERIFY(spyWarning.wait(1000));
    QCOMPARE(spyWarning.count(), 1);
    QCOMPARE(spyWarning.first().first().toString(), QStringLiteral("Voice Command is Failed"));
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    QCOMPARE(spyState.count(), 2);
    QCOMPARE(spyState.at(0).first().value<VoiceCommandManager::SessionState>(),
             VoiceCommandManager::SessionState::Processing);
    QCOMPARE(spyState.at(1).first().value<VoiceCommandManager::SessionState>(),
             VoiceCommandManager::SessionState::Idle);
    QCOMPARE(spyConfirmation.count(), 1);
}

void VoiceCommandManagerTest::_testIntentMappingFailure()
{
    ignoreLogMessage("VoiceCommand.VoiceCommandManager", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Voice intent mapping failed:.*|STT transcription failed:.*")));

    VoiceCommandManager* manager = VoiceCommandManager::instance();
    RejectingParser parser;
    manager->setIntentParser(&parser);
    QSignalSpy warning(manager, &VoiceCommandManager::voiceCommandWarning);
    QSignalSpy confirmation(manager, &VoiceCommandManager::commandConfirmationRequested);
    QSignalSpy states(manager, &VoiceCommandManager::sessionStateChanged);

    manager->_setSessionState(VoiceCommandManager::SessionState::Processing);
    states.clear();
    manager->_onTranscriptionFinished({true, QStringLiteral("unknown command"), 0.9f, 10, {}});
    QCOMPARE(warning.size(), 1);
    QCOMPARE(warning.first().first().toString(), QStringLiteral("Voice Command is Failed"));
    QCOMPARE(confirmation.size(), 0);
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    QCOMPARE(states.size(), 1);
    QCOMPARE(states.first().first().value<VoiceCommandManager::SessionState>(),
             VoiceCommandManager::SessionState::Idle);

    manager->setIntentParser(nullptr);
    manager->_setSessionState(VoiceCommandManager::SessionState::Processing);
    states.clear();
    manager->_onTranscriptionFinished({true, QStringLiteral("not a command"), 0.9f, 10, {}});
    QCOMPARE(warning.size(), 2);
    QCOMPARE(confirmation.size(), 0);
    QCOMPARE(states.size(), 1);
    QCOMPARE(states.first().first().value<VoiceCommandManager::SessionState>(),
             VoiceCommandManager::SessionState::Idle);

    manager->_setSessionState(VoiceCommandManager::SessionState::Processing);
    states.clear();
    manager->_onTranscriptionFinished({true, QStringLiteral("  "), 0.9f, 10, {}});
    QCOMPARE(warning.size(), 3);
    QCOMPARE(confirmation.size(), 0);
    QCOMPARE(states.size(), 1);
    QCOMPARE(states.first().first().value<VoiceCommandManager::SessionState>(),
             VoiceCommandManager::SessionState::Idle);

    // Resetting the injected, non-owning parser restores the built-in matcher.
    manager->_onTranscriptionFinished({true, QStringLiteral("arm"), 0.9f, 10, {}});
    QCOMPARE(confirmation.size(), 1);
    QCOMPARE(confirmation.first().first().value<IntentParseResult>().intents.first().action, VoiceCommandAction::Arm);
}

void VoiceCommandManagerTest::_testRecognitionTestTranscription()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    auto* engine = new MockSttEngine();
    engine->_mockResult = {true, QStringLiteral("  this is only a test  "), 0.9f, 10, {}};
    manager->setSttEngine(engine, true);

    QSignalSpy transcribed(manager, &VoiceCommandManager::voiceCommandTranscribed);
    QSignalSpy confirmation(manager, &VoiceCommandManager::commandConfirmationRequested);
    QSignalSpy resultChanged(manager, &VoiceCommandManager::recognitionTestResultChanged);
    QSignalSpy activeChanged(manager, &VoiceCommandManager::recognitionTestActiveChanged);
    QSignalSpy warnings(manager, &VoiceCommandManager::voiceCommandWarning);

    // Simulate a completed settings test; no microphone or vehicle is needed for inference.
    manager->_isRecognitionTestActive = true;
    QCOMPARE(manager->startRecording(), false);
    QAudioFormat format = AudioCapture::canonicalAudioFormat();
    manager->_onRecordingFinished(QByteArray(16000, '\0'), format);

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Processing);
    QTRY_COMPARE_WITH_TIMEOUT(manager->isRecognitionTestActive(), false, 1000);
    QCOMPARE(activeChanged.count(), 1);
    QCOMPARE(manager->recognitionTestTranscript(), QStringLiteral("this is only a test"));
    QVERIFY(manager->recognitionTestError().isEmpty());
    QVERIFY(manager->recognitionTestIntent().isEmpty());
    QCOMPARE(manager->isRecognitionTestActive(), false);
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    QCOMPARE(resultChanged.count(), 1);
    QCOMPARE(transcribed.count(), 0);
    QCOMPARE(confirmation.count(), 0);
    QCOMPARE(warnings.count(), 0);

    // A complete sequence is previewed by the built-in matcher, not the injected command parser.
    RejectingParser parser;
    manager->setIntentParser(&parser);
    manager->_isRecognitionTestActive = true;
    manager->_onTranscriptionFinished({true, QStringLiteral("arm and then take off to 5 meters"), 0.9f, 10, {}});
    QCOMPARE(manager->recognitionTestTranscript(), QStringLiteral("arm and then take off to 5 meters"));
    QCOMPARE(manager->recognitionTestIntent(), QStringLiteral("Arm → Take off to 5 m"));
    QCOMPARE(resultChanged.count(), 2);
    QCOMPARE(confirmation.count(), 0);
    QCOMPARE(warnings.count(), 0);

    // A partial match must not show a misleading action.
    manager->_isRecognitionTestActive = true;
    manager->_onTranscriptionFinished({true, QStringLiteral("arm and then nonsense"), 0.9f, 10, {}});
    QCOMPARE(manager->recognitionTestTranscript(), QStringLiteral("arm and then nonsense"));
    QVERIFY(manager->recognitionTestIntent().isEmpty());
    QCOMPARE(confirmation.count(), 0);
    manager->setIntentParser(nullptr);
}

void VoiceCommandManagerTest::_testRecognitionTestErrors()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QSignalSpy warnings(manager, &VoiceCommandManager::voiceCommandWarning);
    QSignalSpy confirmation(manager, &VoiceCommandManager::commandConfirmationRequested);
    QAudioFormat format = AudioCapture::canonicalAudioFormat();

    manager->_isRecognitionTestActive = true;
    manager->_onTranscriptionFinished({true, QStringLiteral("arm"), 0.9f, 10, {}});
    QCOMPARE(manager->recognitionTestIntent(), QStringLiteral("Arm"));

    manager->_isRecognitionTestActive = true;
    manager->_onRecordingFinished(QByteArray(8000, '\0'), format);
    QVERIFY(!manager->isRecognitionTestActive());
    QVERIFY(manager->recognitionTestError().contains(QStringLiteral("too short")));
    QVERIFY(manager->recognitionTestIntent().isEmpty());
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    QCOMPARE(warnings.count(), 0);

    manager->_isRecognitionTestActive = true;
    manager->_onTranscriptionFinished({false, {}, 0.0f, 10, QStringLiteral("Model error")});
    QCOMPARE(manager->recognitionTestError(), QStringLiteral("Speech recognition failed: Model error"));
    QVERIFY(manager->recognitionTestTranscript().isEmpty());
    QVERIFY(manager->recognitionTestIntent().isEmpty());
    QCOMPARE(confirmation.count(), 0);
    QCOMPARE(warnings.count(), 0);

    manager->_isRecognitionTestActive = true;
    manager->_onTranscriptionFinished({true, QStringLiteral("  "), 0.0f, 10, {}});
    QCOMPARE(manager->recognitionTestError(), QStringLiteral("No speech recognized. Try speaking again."));
    QVERIFY(manager->recognitionTestIntent().isEmpty());
    QCOMPARE(confirmation.count(), 0);
    QCOMPARE(warnings.count(), 0);
}

void VoiceCommandManagerTest::_testRecognitionTestRecordingWithoutVehicle()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    manager->setVehicleCheckEnabled(true);
    QCOMPARE(manager->isVehicleConnected(), false);

    if (!manager->isMicrophoneActive()) {
        QCOMPARE(manager->startRecognitionTest(), false);
        return;
    }

    manager->setSttEngine(new MockSttEngine(), true);
    QSignalSpy confirmation(manager, &VoiceCommandManager::commandConfirmationRequested);
    QAudioFormat format = AudioCapture::canonicalAudioFormat();
    QByteArray pcm(16000, '\0');
    QBuffer input(&pcm);
    AudioCapture::instance()->setAudioInputDeviceForTesting(&input, format);

    manager->_isRecognitionTestActive = true;
    manager->_onTranscriptionFinished({true, QStringLiteral("land"), 0.9f, 10, {}});
    QCOMPARE(manager->recognitionTestIntent(), QStringLiteral("Land"));

    QVERIFY(manager->startRecognitionTest());
    QVERIFY(manager->isRecognitionTestActive());
    QVERIFY(manager->isRecording());
    QVERIFY(manager->recognitionTestIntent().isEmpty());
    manager->stopRecognitionTest();
    QTRY_COMPARE_WITH_TIMEOUT(manager->isRecognitionTestActive(), false, 1000);
    QCOMPARE(manager->recognitionTestTranscript(), QStringLiteral("arm the drone"));
    QCOMPARE(manager->recognitionTestIntent(), QStringLiteral("Arm"));
    QCOMPARE(confirmation.count(), 0);
    AudioCapture::instance()->setAudioInputDeviceForTesting(nullptr);
}

void VoiceCommandManagerTest::_testConfirmationApproveFlow()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QSignalSpy confirmation(manager, &VoiceCommandManager::commandConfirmationRequested);
    QSignalSpy approved(manager, &VoiceCommandManager::commandApproved);
    QSignalSpy rejected(manager, &VoiceCommandManager::commandRejected);
    QSignalSpy pendingChanged(manager, &VoiceCommandManager::pendingIntentsChanged);

    manager->_onTranscriptionFinished({true, QStringLiteral("takeoff"), 0.95f, 10, {}});

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Confirming);
    QVERIFY(manager->hasPendingConfirmation());
    QVERIFY(!manager->pendingIntents().isEmpty());
    QCOMPARE(manager->pendingIntentsVariant().size(), manager->pendingIntents().size());
    QCOMPARE(manager->pendingTranscript(), QStringLiteral("takeoff"));
    QCOMPARE(confirmation.count(), 1);
    QCOMPARE(pendingChanged.count(), 1);

    // Cannot start a new recording while awaiting operator confirmation
    QCOMPARE(manager->startRecording(), false);

    const QList<MissionIntent> expectedIntents = manager->pendingIntents();
    manager->approvePendingCommand();

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Executing);
    QVERIFY(!manager->hasPendingConfirmation());
    QVERIFY(manager->pendingIntents().isEmpty());
    QVERIFY(manager->pendingTranscript().isEmpty());
    QCOMPARE(approved.count(), 1);
    QCOMPARE(approved.takeFirst().at(0).value<QList<MissionIntent>>(), expectedIntents);
    QCOMPARE(rejected.count(), 0);
}

void VoiceCommandManagerTest::_testConfirmationRejectFlow()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QSignalSpy approved(manager, &VoiceCommandManager::commandApproved);
    QSignalSpy rejected(manager, &VoiceCommandManager::commandRejected);

    manager->_onTranscriptionFinished({true, QStringLiteral("land"), 0.92f, 10, {}});

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Confirming);
    QVERIFY(manager->hasPendingConfirmation());
    QVERIFY(!manager->pendingIntents().isEmpty());

    manager->rejectPendingCommand();

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Idle);
    QVERIFY(!manager->hasPendingConfirmation());
    QVERIFY(manager->pendingIntents().isEmpty());
    QVERIFY(manager->pendingTranscript().isEmpty());
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(approved.count(), 0);
}

void VoiceCommandManagerTest::_testConfirmationStepToggling()
{
    VoiceCommandManager* manager = VoiceCommandManager::instance();
    QSignalSpy approved(manager, &VoiceCommandManager::commandApproved);

    manager->_onTranscriptionFinished(
        {true, QStringLiteral("arm and then take off to 5 meters and then land"), 0.95f, 10, {}});

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Confirming);
    QCOMPARE(manager->pendingIntents().size(), 3);
    QCOMPARE(manager->activePendingIntentCount(), 3);
    QCOMPARE(manager->pendingIntentEnabledVariant(), QVariantList({true, true, true}));

    // Disable all steps -> approve must be ignored and session remains Confirming
    manager->togglePendingIntent(0);
    manager->togglePendingIntent(1);
    manager->togglePendingIntent(2);
    QCOMPARE(manager->activePendingIntentCount(), 0);
    manager->approvePendingCommand();
    QCOMPARE(approved.count(), 0);
    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Confirming);

    // Re-enable index 0 (Arm) and index 2 (Land), leaving index 1 (Takeoff) skipped
    manager->togglePendingIntent(0);
    manager->togglePendingIntent(2);
    QCOMPARE(manager->activePendingIntentCount(), 2);
    QCOMPARE(manager->pendingIntentEnabledVariant(), QVariantList({true, false, true}));

    const QList<MissionIntent> allIntents = manager->pendingIntents();
    manager->approvePendingCommand();

    QCOMPARE(manager->sessionState(), VoiceCommandManager::SessionState::Executing);
    QCOMPARE(approved.count(), 1);
    const QList<MissionIntent> emitted = approved.takeFirst().at(0).value<QList<MissionIntent>>();
    QCOMPARE(emitted.size(), 2);
    QCOMPARE(emitted.at(0), allIntents.at(0));
    QCOMPARE(emitted.at(1), allIntents.at(2));
}

UT_REGISTER_TEST(VoiceCommandManagerTest, TestLabel::Unit, TestLabel::Utilities)
