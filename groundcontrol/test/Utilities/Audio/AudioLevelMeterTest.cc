#include "AudioLevelMeterTest.h"

#include <QtTest/QSignalSpy>
#include <cmath>
#include <numbers>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "AudioLevelMeter.h"

void AudioLevelMeterTest::init()
{
    UnitTest::init();
}

void AudioLevelMeterTest::cleanup()
{
    AudioLevelMeter::instance()->stopTest();
    UnitTest::cleanup();
}

void AudioLevelMeterTest::_testInitialState()
{
    AudioLevelMeter* meter = AudioLevelMeter::instance();
    QVERIFY(meter != nullptr);

    QVERIFY(!meter->isTesting());
    QCOMPARE(meter->audioLevel(), 0.0);
    QCOMPARE(meter->peakLevel(), 0.0);
    QCOMPARE(meter->bandCount(), AudioLevelMeter::kDefaultBandCount);
    QCOMPARE(meter->frequencyBands().size(), AudioLevelMeter::kDefaultBandCount);

    for (qreal b : meter->frequencyBands()) {
        QCOMPARE(b, 0.0);
    }
}

void AudioLevelMeterTest::_testTestToggleLifecycle()
{
    AudioLevelMeter* meter = AudioLevelMeter::instance();
    AudioDeviceManager* manager = AudioDeviceManager::instance();

    QSignalSpy testingSpy(meter, &AudioLevelMeter::isTestingChanged);

    if (manager->isMicrophoneActive()) {
        QVERIFY(meter->startTest());
        QVERIFY(meter->isTesting());
        QCOMPARE(testingSpy.count(), 1);

        meter->stopTest();
        QVERIFY(!meter->isTesting());
        QCOMPARE(testingSpy.count(), 2);

        meter->toggleTest();
        QVERIFY(meter->isTesting());
        QCOMPARE(testingSpy.count(), 3);

        meter->toggleTest();
        QVERIFY(!meter->isTesting());
        QCOMPARE(testingSpy.count(), 4);
    } else {
        QVERIFY(!meter->startTest());
        QVERIFY(!meter->isTesting());
    }
}

void AudioLevelMeterTest::_testLevelCalculationFromStream()
{
    AudioLevelMeter* meter = AudioLevelMeter::instance();
    QSignalSpy levelsSpy(meter, &AudioLevelMeter::levelsChanged);

    meter->_isTesting = true;

    // Synthetic 16-bit PCM buffer at 16000 Hz, 1 channel
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    // Loud 1000 Hz sine wave
    const int sampleCount = 320;  // 20ms
    QByteArray pcmData;
    pcmData.resize(sampleCount * sizeof(qint16));
    auto* samples = reinterpret_cast<qint16*>(pcmData.data());

    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / 16000.0f;
        const float val = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * t);
        samples[i] = static_cast<qint16>(val * 28000.0f);
    }

    meter->_processAudioBuffer(pcmData, format);

    QVERIFY(levelsSpy.count() >= 1);
    QVERIFY(meter->audioLevel() > 0.4);
    QVERIFY(meter->peakLevel() > 0.7);

    meter->stopTest();
}

void AudioLevelMeterTest::_testGoertzelFrequencyBands()
{
    AudioLevelMeter* meter = AudioLevelMeter::instance();
    meter->_isTesting = true;

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    // Generate pure 1000 Hz tone (Band 3 center is 1000 Hz)
    const int sampleCount = 480;
    QByteArray pcmData;
    pcmData.resize(sampleCount * sizeof(qint16));
    auto* samples = reinterpret_cast<qint16*>(pcmData.data());

    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / 16000.0f;
        const float val = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * t);
        samples[i] = static_cast<qint16>(val * 25000.0f);
    }

    meter->_processAudioBuffer(pcmData, format);

    const QList<qreal> bands = meter->frequencyBands();
    QCOMPARE(bands.size(), AudioLevelMeter::kDefaultBandCount);

    QVERIFY(bands[7] > 0.2);
    QVERIFY(bands[7] > bands[0]);
    QVERIFY(bands[7] > bands[15]);

    meter->stopTest();
}

void AudioLevelMeterTest::_testDecayBehavior()
{
    AudioLevelMeter* meter = AudioLevelMeter::instance();
    meter->_isTesting = true;
    meter->_audioLevel = 0.8;
    meter->_peakLevel = 0.9;
    meter->_frequencyBands[7] = 0.7;

    const qreal prevLevel = meter->audioLevel();
    const qreal prevBand = meter->frequencyBands()[7];

    meter->_decayLevels();

    QVERIFY(meter->audioLevel() < prevLevel);
    QVERIFY(meter->frequencyBands()[7] < prevBand);

    meter->stopTest();
}

void AudioLevelMeterTest::_testResetLevels()
{
    AudioLevelMeter* meter = AudioLevelMeter::instance();
    meter->_audioLevel = 0.8;
    meter->_peakLevel = 0.9;
    meter->_frequencyBands[1] = 0.6;

    meter->_resetLevels();

    QCOMPARE(meter->audioLevel(), 0.0);
    QCOMPARE(meter->peakLevel(), 0.0);
    for (qreal b : meter->frequencyBands()) {
        QCOMPARE(b, 0.0);
    }
}
