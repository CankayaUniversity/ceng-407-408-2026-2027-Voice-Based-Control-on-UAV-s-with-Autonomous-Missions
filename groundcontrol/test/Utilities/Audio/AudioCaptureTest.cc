#include "AudioCaptureTest.h"

#include <QtCore/QBuffer>
#include <QtTest/QSignalSpy>
#include <cmath>
#include <memory>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "DspNoiseFilter.h"

void AudioCaptureTest::init()
{
    UnitTest::init();
    ignoreLogMessage("qt.multimedia.pipewire.registry", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to connect to pipewire")));
    ignoreLogMessage("default", QtWarningMsg, QRegularExpression(QStringLiteral("PulseAudioService|secure directory")));
    ignoreLogMessage(
        "Utilities.AudioCapture", QtWarningMsg,
        QRegularExpression(QStringLiteral("Recording safety limit reached|Pre-empting active streaming|Cannot start "
                                          "capture|Active microphone disconnected")));
}

void AudioCaptureTest::cleanup()
{
    AudioCapture::instance()->setAudioInputDeviceForTesting(nullptr);
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
    QVERIFY(capture->isNoiseFilterEnabled());
    QCOMPARE(capture->noiseFilterLevel(), NoiseFilterInterface::Level::Moderate);
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

void AudioCaptureTest::_testCaptureLifecycleWithTestSeam()
{
    AudioCapture* capture = AudioCapture::instance();

    // 16000 Hz, 1 channel, Int16 PCM (50 ms = 800 samples = 1600 bytes)
    const QAudioFormat format = AudioCapture::canonicalAudioFormat();
    constexpr int sampleCount = 800;
    QByteArray pcmData(sampleCount * sizeof(qint16), 0);
    auto* samples = reinterpret_cast<qint16*>(pcmData.data());
    for (int i = 0; i < sampleCount; ++i) {
        samples[i] = static_cast<qint16>(std::sin(2.0 * M_PI * 440.0 * (i / 16000.0)) * 10000.0);
    }

    QBuffer testBuffer(&pcmData);
    capture->setAudioInputDeviceForTesting(&testBuffer, format);

    QSignalSpy stateSpy(capture, &AudioCapture::captureStateChanged);
    QSignalSpy finishedSpy(capture, &AudioCapture::recordingFinished);

    QVERIFY(capture->startRecording());
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Recording);
    QVERIFY(capture->isCapturing());
    QVERIFY(capture->isRecording());
    QVERIFY(!capture->isStreaming());

    const QByteArray recorded = capture->stopRecording();
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
    QVERIFY(!capture->isCapturing());
    QCOMPARE(finishedSpy.count(), 1);

    const auto args = finishedSpy.takeFirst();
    const QAudioFormat emittedFormat = args.at(1).value<QAudioFormat>();
    QCOMPARE(emittedFormat.sampleRate(), 16000);
    QCOMPARE(emittedFormat.channelCount(), 1);
    QCOMPARE(emittedFormat.sampleFormat(), QAudioFormat::Int16);
    QCOMPARE(recorded.size(), pcmData.size());
}

void AudioCaptureTest::_testStreamingLifecycleWithTestSeam()
{
    AudioCapture* capture = AudioCapture::instance();

    const QAudioFormat format = AudioCapture::canonicalAudioFormat();
    constexpr int sampleCount = 320;
    QByteArray pcmData(sampleCount * sizeof(qint16), 0);
    auto* samples = reinterpret_cast<qint16*>(pcmData.data());
    for (int i = 0; i < sampleCount; ++i) {
        samples[i] = static_cast<qint16>(1000);
    }

    QBuffer testBuffer;
    (void) testBuffer.open(QIODevice::ReadWrite);
    capture->setAudioInputDeviceForTesting(&testBuffer, format);

    QSignalSpy streamSpy(capture, &AudioCapture::audioStreamFrameReady);

    QVERIFY(capture->startStreaming());
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Streaming);
    QVERIFY(capture->isStreaming());

    (void) testBuffer.write(pcmData);
    (void) testBuffer.seek(0);
    capture->_onReadyRead();

    QVERIFY(streamSpy.count() >= 1);
    const auto args = streamSpy.takeFirst();
    const QAudioFormat emittedFormat = args.at(1).value<QAudioFormat>();
    QCOMPARE(emittedFormat.sampleRate(), 16000);
    QCOMPARE(emittedFormat.channelCount(), 1);
    QCOMPARE(emittedFormat.sampleFormat(), QAudioFormat::Int16);

    capture->stopStreaming();
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
}

void AudioCaptureTest::_testConflictPreemption()
{
    AudioCapture* capture = AudioCapture::instance();

    const QAudioFormat format = AudioCapture::canonicalAudioFormat();
    QBuffer testBuffer;
    (void) testBuffer.open(QIODevice::ReadWrite);
    capture->setAudioInputDeviceForTesting(&testBuffer, format);

    QSignalSpy warningSpy(capture, &AudioCapture::captureWarning);
    QSignalSpy stateSpy(capture, &AudioCapture::captureStateChanged);

    // 1. Start streaming first (e.g. mic test)
    QVERIFY(capture->startStreaming());
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Streaming);

    // 2. Trigger recording while streaming is active
    QVERIFY(capture->startRecording());

    // 3. Verify pre-emption warning was emitted and state transitioned to Recording
    QCOMPARE(warningSpy.count(), 1);
    const QString warningMsg = warningSpy.takeFirst().at(0).toString();
    QVERIFY(warningMsg.contains(QStringLiteral("pre-empted"), Qt::CaseInsensitive));
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Recording);

    (void) capture->stopRecording();
    QCOMPARE(capture->captureState(), AudioCapture::CaptureState::Stopped);
}

void AudioCaptureTest::_testDownmixStereoToMono()
{
    AudioCapture* capture = AudioCapture::instance();
    capture->setNoiseFilterEnabled(false);

    // Create 16 kHz Stereo Int16 buffer: Left = 10000, Right = 20000
    // Downmixed mono frame should average to: (10000 + 20000) / 2 = 15000
    QAudioFormat stereoFormat;
    stereoFormat.setSampleRate(16000);
    stereoFormat.setChannelCount(2);
    stereoFormat.setSampleFormat(QAudioFormat::Int16);

    constexpr int frameCount = 400;
    QByteArray stereoData;
    stereoData.resize(frameCount * 2 * sizeof(qint16));
    auto* src = reinterpret_cast<qint16*>(stereoData.data());
    for (int i = 0; i < frameCount; ++i) {
        src[i * 2 + 0] = 10000;
        src[i * 2 + 1] = 20000;
    }

    QBuffer testBuffer(&stereoData);
    capture->setAudioInputDeviceForTesting(&testBuffer, stereoFormat);

    QVERIFY(capture->startRecording());
    const QByteArray recorded = capture->stopRecording();

    // Mono Int16 is frameCount * sizeof(qint16)
    QCOMPARE(recorded.size(), frameCount * static_cast<int>(sizeof(qint16)));
    const auto* monoSamples = reinterpret_cast<const qint16*>(recorded.constData());
    for (int i = 0; i < frameCount; ++i) {
        QVERIFY(std::abs(monoSamples[i] - 15000) <= 2);
    }

    capture->setNoiseFilterEnabled(true);
}

void AudioCaptureTest::_testResamplingTo16kHz()
{
    AudioCapture* capture = AudioCapture::instance();
    capture->setNoiseFilterEnabled(false);

    // 1. Resample 48 kHz Mono Int16 -> 16 kHz Mono Int16
    {
        QAudioFormat format48k;
        format48k.setSampleRate(48000);
        format48k.setChannelCount(1);
        format48k.setSampleFormat(QAudioFormat::Int16);

        constexpr int sampleCount48k = 4800;  // 100ms
        QByteArray data48k(sampleCount48k * sizeof(qint16), 0);
        auto* src = reinterpret_cast<qint16*>(data48k.data());
        for (int i = 0; i < sampleCount48k; ++i) {
            src[i] = static_cast<qint16>(std::sin(2.0 * M_PI * 500.0 * (i / 48000.0)) * 12000.0);
        }

        QBuffer buffer48k(&data48k);
        capture->setAudioInputDeviceForTesting(&buffer48k, format48k);

        QVERIFY(capture->startRecording());
        const QByteArray recorded = capture->stopRecording();

        // 100ms at 16 kHz = 1600 samples = 3200 bytes
        const int expectedBytes = 1600 * sizeof(qint16);
        QVERIFY(std::abs(recorded.size() - expectedBytes) <= 4);
    }

    // 2. Resample 44.1 kHz Mono Float -> 16 kHz Mono Int16
    {
        QAudioFormat format44k;
        format44k.setSampleRate(44100);
        format44k.setChannelCount(1);
        format44k.setSampleFormat(QAudioFormat::Float);

        constexpr int sampleCount44k = 4410;  // 100ms
        QByteArray data44k(sampleCount44k * sizeof(float), 0);
        auto* src = reinterpret_cast<float*>(data44k.data());
        for (int i = 0; i < sampleCount44k; ++i) {
            src[i] = static_cast<float>(std::sin(2.0 * M_PI * 500.0 * (i / 44100.0)) * 0.4);
        }

        QBuffer buffer44k(&data44k);
        capture->setAudioInputDeviceForTesting(&buffer44k, format44k);

        QVERIFY(capture->startRecording());
        const QByteArray recorded = capture->stopRecording();

        const int expectedBytes = 1600 * sizeof(qint16);
        QVERIFY(std::abs(recorded.size() - expectedBytes) <= 4);
    }

    capture->setNoiseFilterEnabled(true);
}

void AudioCaptureTest::_testNoiseFilterSettings()
{
    AudioCapture* capture = AudioCapture::instance();
    QSignalSpy enabledSpy(capture, &AudioCapture::noiseFilterEnabledChanged);
    QSignalSpy levelSpy(capture, &AudioCapture::noiseFilterLevelChanged);

    const bool initialEnabled = capture->isNoiseFilterEnabled();
    capture->setNoiseFilterEnabled(!initialEnabled);
    QCOMPARE(capture->isNoiseFilterEnabled(), !initialEnabled);
    QCOMPARE(enabledSpy.count(), 1);

    capture->setNoiseFilterEnabled(initialEnabled);
    QCOMPARE(capture->isNoiseFilterEnabled(), initialEnabled);
    QCOMPARE(enabledSpy.count(), 2);

    capture->setNoiseFilterLevel(NoiseFilterInterface::Level::High);
    QCOMPARE(capture->noiseFilterLevel(), NoiseFilterInterface::Level::High);
    QCOMPARE(capture->noiseFilterLevelInt(), static_cast<int>(NoiseFilterInterface::Level::High));
    QCOMPARE(levelSpy.count(), 1);

    capture->setNoiseFilterLevelInt(static_cast<int>(NoiseFilterInterface::Level::Low));
    QCOMPARE(capture->noiseFilterLevel(), NoiseFilterInterface::Level::Low);
    QCOMPARE(capture->noiseFilterLevelInt(), 0);

    capture->setNoiseFilterLevel(NoiseFilterInterface::Level::Moderate);
}

void AudioCaptureTest::_testNoiseFilterSwapping()
{
    AudioCapture* capture = AudioCapture::instance();
    QVERIFY(capture->noiseFilter() != nullptr);

    class MockCustomFilter : public NoiseFilterInterface
    {
    public:
        Level level() const override { return _level; }

        void setLevel(Level lvl) override { _level = lvl; }

        void reset() override {}

        void process(QByteArray&, const QAudioFormat&) override {}

        QString name() const override { return QStringLiteral("MockCustomFilter"); }

    private:
        Level _level = Level::Moderate;
    };

    capture->setNoiseFilter(std::make_unique<MockCustomFilter>());
    QCOMPARE(capture->noiseFilter()->name(), QStringLiteral("MockCustomFilter"));

    // Restore standard DSP filter (user confirmed name is "DSP")
    capture->setNoiseFilter(std::make_unique<DspNoiseFilter>());
    QCOMPARE(capture->noiseFilter()->name(), QStringLiteral("DSP"));
}

void AudioCaptureTest::_testNoiseFilterProcessingWithInjectedAudio()
{
    AudioCapture* capture = AudioCapture::instance();
    capture->setNoiseFilterEnabled(true);
    capture->setNoiseFilterLevel(NoiseFilterInterface::Level::High);

    // Create low-level background noise at 16 kHz Mono Int16 (amplitude ~80)
    const QAudioFormat format = AudioCapture::canonicalAudioFormat();
    constexpr int sampleCount = 3200;  // 200ms
    QByteArray pcmData(sampleCount * sizeof(qint16), 0);
    auto* src = reinterpret_cast<qint16*>(pcmData.data());
    for (int i = 0; i < sampleCount; ++i) {
        src[i] = (i % 2 == 0) ? 80 : -80;
    }

    QBuffer testBuffer(&pcmData);
    capture->setAudioInputDeviceForTesting(&testBuffer, format);

    QVERIFY(capture->startRecording());
    const QByteArray recorded = capture->stopRecording();

    // Verify noise gate attenuated the low amplitude noise
    const auto* outSamples = reinterpret_cast<const qint16*>(recorded.constData());
    const int outCount = recorded.size() / sizeof(qint16);
    qint64 sumSquares = 0;
    const int startIdx = outCount / 2;
    for (int i = startIdx; i < outCount; ++i) {
        sumSquares += static_cast<qint64>(outSamples[i]) * outSamples[i];
    }
    const double rms = std::sqrt(static_cast<double>(sumSquares) / (outCount - startIdx));
    QVERIFY(rms < 80.0);
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

    const QAudioFormat emittedFormat = args.at(1).value<QAudioFormat>();
    QCOMPARE(emittedFormat.sampleRate(), 16000);
    QCOMPARE(emittedFormat.channelCount(), 1);
    QCOMPARE(emittedFormat.sampleFormat(), QAudioFormat::Int16);
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
