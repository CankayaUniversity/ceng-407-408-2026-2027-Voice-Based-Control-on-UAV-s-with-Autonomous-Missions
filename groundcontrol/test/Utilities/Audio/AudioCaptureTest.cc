#include "AudioCaptureTest.h"

#include <QtTest/QSignalSpy>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"

void AudioCaptureTest::init()
{
    UnitTest::init();
    ignoreLogMessage("qt.multimedia.pipewire.registry", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to connect to pipewire")));
    ignoreLogMessage("default", QtWarningMsg, QRegularExpression(QStringLiteral("PulseAudioService|secure directory")));
}

void AudioCaptureTest::cleanup()
{
    AudioCapture::instance()->stopCapture();
    UnitTest::cleanup();
}

void AudioCaptureTest::_testInitialState()
{
    AudioCapture* capture = AudioCapture::instance();
    QVERIFY(capture != nullptr);

    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
    QVERIFY(!capture->isCapturing());
    QVERIFY(!capture->isRecording());
    QVERIFY(!capture->isStreaming());
    QCOMPARE(capture->noiseFilterStrength(), 50);
}

void AudioCaptureTest::_testTargetFormat()
{
    AudioCapture* capture = AudioCapture::instance();
    const QAudioFormat defaultFormat = capture->targetAudioFormat();

    QCOMPARE(defaultFormat.sampleRate(), 16000);
    QCOMPARE(defaultFormat.channelCount(), 1);
    QCOMPARE(defaultFormat.sampleFormat(), QAudioFormat::Int16);

    QAudioFormat customFormat;
    customFormat.setSampleRate(44100);
    customFormat.setChannelCount(2);
    customFormat.setSampleFormat(QAudioFormat::Float);

    capture->setTargetAudioFormat(customFormat);
    QCOMPARE(capture->targetAudioFormat(), customFormat);

    // Reset back to speech default
    capture->setTargetAudioFormat(defaultFormat);
    QCOMPARE(capture->targetAudioFormat(), defaultFormat);
}

void AudioCaptureTest::_testCaptureLifecycle()
{
    AudioCapture* capture = AudioCapture::instance();
    AudioDeviceManager* manager = AudioDeviceManager::instance();

    if (manager->isMicrophoneActive()) {
        // Test Recording mode
        QVERIFY(capture->startRecording());
        QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Recording);
        QVERIFY(capture->isCapturing());
        QVERIFY(capture->isRecording());
        QVERIFY(!capture->isStreaming());

        const QByteArray buffer = capture->stopRecording();
        QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
        QVERIFY(!capture->isCapturing());
    } else {
        // Without active microphone, capture should gracefully fail
        QVERIFY(!capture->startRecording());
        QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
    }
}

void AudioCaptureTest::_testStreamingLifecycle()
{
    AudioCapture* capture = AudioCapture::instance();
    AudioDeviceManager* manager = AudioDeviceManager::instance();

    if (manager->isMicrophoneActive()) {
        QVERIFY(capture->startStreaming());
        QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Streaming);
        QVERIFY(capture->isCapturing());
        QVERIFY(capture->isStreaming());
        QVERIFY(!capture->isRecording());

        capture->stopStreaming();
        QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
        QVERIFY(!capture->isCapturing());
        QVERIFY(!capture->isStreaming());
    } else {
        QVERIFY(!capture->startStreaming());
        QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
    }
}

void AudioCaptureTest::_testNoiseFilterStrength()
{
    AudioCapture* capture = AudioCapture::instance();
    QSignalSpy strengthSpy(capture, &AudioCapture::noiseFilterStrengthChanged);

    capture->setNoiseFilterStrength(75);
    QCOMPARE(capture->noiseFilterStrength(), 75);
    QCOMPARE(strengthSpy.count(), 1);

    capture->setNoiseFilterStrength(150);  // Clamped to 100
    QCOMPARE(capture->noiseFilterStrength(), 100);

    capture->setNoiseFilterStrength(-20);  // Clamped to 0
    QCOMPARE(capture->noiseFilterStrength(), 0);

    capture->setNoiseFilterStrength(50);  // Reset
}

void AudioCaptureTest::_testSafetyLimitReached()
{
    AudioCapture* capture = AudioCapture::instance();
    QSignalSpy safetySpy(capture, &AudioCapture::recordingSafetyLimitReached);

    // Simulate recording active with some buffered data
    capture->_state = AudioCapture::CaptureState::Recording;
    capture->_recordedBuffer = QByteArray(1024, 0x1A);

    capture->_onSafetyLimitReached();

    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
    QCOMPARE(safetySpy.count(), 1);
    const QList<QVariant> args = safetySpy.takeFirst();
    QCOMPARE(args.at(0).toByteArray().size(), 1024);
}

void AudioCaptureTest::_testStreamingFrameEmission()
{
    AudioCapture* capture = AudioCapture::instance();
    QSignalSpy streamSpy(capture, &AudioCapture::audioStreamFrameReady);

    // Simulate Streaming state
    capture->_state = AudioCapture::CaptureState::Streaming;

    // Emit test frame
    const QByteArray testChunk(256, 0x05);
    capture->_actualFormat.setSampleRate(16000);
    capture->_actualFormat.setChannelCount(1);
    capture->_actualFormat.setSampleFormat(QAudioFormat::Int16);

    emit capture->audioStreamFrameReady(testChunk, capture->_actualFormat);

    QCOMPARE(streamSpy.count(), 1);
    capture->_state = AudioCapture::CaptureState::Stopped;
}

void AudioCaptureTest::_testSignalEmissions()
{
    AudioCapture* capture = AudioCapture::instance();
    AudioDeviceManager* manager = AudioDeviceManager::instance();

    QSignalSpy stateSpy(capture, &AudioCapture::captureStateChanged);
    QSignalSpy errorSpy(capture, &AudioCapture::captureError);

    if (manager->isMicrophoneActive()) {
        capture->startStreaming();
        QVERIFY(stateSpy.count() >= 1);

        capture->stopCapture();
        QVERIFY(stateSpy.count() >= 2);
    } else {
        capture->startStreaming();
        QVERIFY(errorSpy.count() >= 1);
    }
}

void AudioCaptureTest::_testDeviceDisconnectionHandling()
{
    AudioCapture* capture = AudioCapture::instance();

    // Simulate device becoming inactive during streaming
    capture->_state = AudioCapture::CaptureState::Streaming;
    QSignalSpy errorSpy(capture, &AudioCapture::captureError);

    capture->_onDeviceAvailabilityChanged(false);

    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
    QCOMPARE(errorSpy.count(), 1);
}

UT_REGISTER_TEST(AudioCaptureTest, TestLabel::Unit, TestLabel::Utilities)
