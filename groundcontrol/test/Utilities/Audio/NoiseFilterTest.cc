#include "NoiseFilterTest.h"

#include <algorithm>
#include <cmath>

#include "NoiseFilter.h"

void NoiseFilterTest::init()
{
    UnitTest::init();
}

void NoiseFilterTest::cleanup()
{
    UnitTest::cleanup();
}

void NoiseFilterTest::_testDefaultAndSetStrength()
{
    NoiseFilter filter;
    QCOMPARE(filter.strength(), 50);

    filter.setStrength(80);
    QCOMPARE(filter.strength(), 80);

    filter.setStrength(-10);
    QCOMPARE(filter.strength(), 0);

    filter.setStrength(150);
    QCOMPARE(filter.strength(), 100);
}

void NoiseFilterTest::_testBypassWhenZero()
{
    NoiseFilter filter;
    filter.setStrength(0);  // Bypass

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QByteArray pcmData;
    pcmData.resize(256 * sizeof(qint16));
    auto* samples = reinterpret_cast<qint16*>(pcmData.data());
    for (int i = 0; i < 256; ++i) {
        samples[i] = static_cast<qint16>(i * 50);
    }

    const QByteArray originalData = pcmData;
    filter.process(pcmData, format);

    QCOMPARE(pcmData, originalData);
}

void NoiseFilterTest::_testHighPassFilterRumbleSuppression()
{
    NoiseFilter filter;
    filter.setStrength(100);

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);

    constexpr int kSampleCount = 1600;  // 100 ms

    // 1. Generate 30 Hz sub-bass sine wave (should be heavily attenuated by 80Hz highpass)
    QByteArray rumblePcm;
    rumblePcm.resize(kSampleCount * sizeof(float));
    auto* rumbleSamples = reinterpret_cast<float*>(rumblePcm.data());
    for (int i = 0; i < kSampleCount; ++i) {
        const float t = static_cast<float>(i) / 16000.0f;
        rumbleSamples[i] = 0.8f * std::sin(2.0f * static_cast<float>(M_PI) * 30.0f * t);
    }

    filter.reset();
    filter.process(rumblePcm, format);

    // Measure peak after highpass settles
    float rumblePeak = 0.0f;
    for (int i = 800; i < kSampleCount; ++i) {
        rumblePeak = std::max(rumblePeak, std::abs(rumbleSamples[i]));
    }

    // 2. Generate 1000 Hz mid-range sine wave (should pass through clearly)
    QByteArray voicePcm;
    voicePcm.resize(kSampleCount * sizeof(float));
    auto* voiceSamples = reinterpret_cast<float*>(voicePcm.data());
    for (int i = 0; i < kSampleCount; ++i) {
        const float t = static_cast<float>(i) / 16000.0f;
        voiceSamples[i] = 0.8f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * t);
    }

    filter.reset();
    filter.process(voicePcm, format);

    float voicePeak = 0.0f;
    for (int i = 800; i < kSampleCount; ++i) {
        voicePeak = std::max(voicePeak, std::abs(voiceSamples[i]));
    }

    // 30 Hz rumble should be significantly more attenuated than 1000 Hz voice signal
    QVERIFY(rumblePeak < voicePeak);
}

void NoiseFilterTest::_testNoiseGateSuppression()
{
    NoiseFilter filter;
    filter.setStrength(100);

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    constexpr int kSampleCount = 3200;  // 200 ms
    QByteArray lowNoisePcm;
    lowNoisePcm.resize(kSampleCount * sizeof(qint16));
    auto* samples = reinterpret_cast<qint16*>(lowNoisePcm.data());
    for (int i = 0; i < kSampleCount; ++i) {
        samples[i] = (i % 2 == 0) ? 80 : -80;  // Low noise level (~ -52 dB)
    }

    filter.process(lowNoisePcm, format);

    // After settling, low noise should be attenuated
    qint64 sumSquares = 0;
    for (int i = 1600; i < kSampleCount; ++i) {
        sumSquares += static_cast<qint64>(samples[i]) * samples[i];
    }
    const double rms = std::sqrt(static_cast<double>(sumSquares) / 1600.0);

    QVERIFY(rms < 80.0);
}

void NoiseFilterTest::_testReset()
{
    NoiseFilter filter;
    filter.setStrength(75);
    filter._hpZ1 = 1.0f;
    filter._hpZ2 = 2.0f;
    filter._currentGain = 0.5f;

    filter.reset();

    QCOMPARE(filter._hpZ1, 0.0f);
    QCOMPARE(filter._hpZ2, 0.0f);
    QCOMPARE(filter._currentGain, 1.0f);
}
