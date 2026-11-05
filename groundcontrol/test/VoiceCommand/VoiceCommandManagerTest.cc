#include "VoiceCommandManagerTest.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QKeyEvent>
#include <QtTest/QSignalSpy>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "VoiceCommandManager.h"

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
}

void VoiceCommandManagerTest::cleanup()
{
    if (VoiceCommandManager::instance()->isRecording()) {
        VoiceCommandManager::instance()->stopRecording();
    }
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
    QSignalSpy spyRecorded(manager, &VoiceCommandManager::voiceCommandRecorded);

    // Simulate AudioCapture finishing a recording
    const QByteArray testPcm = QByteArrayLiteral("RIFF1234WAVEfmt ");
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

UT_REGISTER_TEST(VoiceCommandManagerTest, TestLabel::Unit, TestLabel::Utilities)
