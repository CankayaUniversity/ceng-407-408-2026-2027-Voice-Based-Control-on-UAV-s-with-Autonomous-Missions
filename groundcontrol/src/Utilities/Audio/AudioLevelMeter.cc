#include "AudioLevelMeter.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AudioLevelMeterLog, "Utilities.AudioLevelMeter")

Q_APPLICATION_STATIC(AudioLevelMeter, _audioLevelMeter);

constexpr float AudioLevelMeter::kCenterFrequencies[AudioLevelMeter::kDefaultBandCount];

AudioLevelMeter::AudioLevelMeter(AudioCapture* capture, AudioDeviceManager* deviceManager, QObject* parent)
    : QObject(parent),
      _capture(capture ? capture : AudioCapture::instance()),
      _deviceManager(deviceManager ? deviceManager : AudioDeviceManager::instance()),
      _decayTimer(new QTimer(this))
{
    _frequencyBands.resize(kDefaultBandCount);
    _frequencyBands.fill(0.0);

    _decayTimer->setInterval(30);
    (void) connect(_decayTimer, &QTimer::timeout, this, &AudioLevelMeter::_decayLevels);

    if (_capture) {
        (void) connect(_capture, &AudioCapture::audioStreamFrameReady, this,
                       &AudioLevelMeter::_onAudioStreamFrameReady);
        (void) connect(_capture, &AudioCapture::captureStateChanged, this, &AudioLevelMeter::_onCaptureStateChanged);
    }

    if (_deviceManager) {
        (void) connect(_deviceManager, &AudioDeviceManager::isMicrophoneActiveChanged, this,
                       &AudioLevelMeter::_onMicrophoneActiveChanged);
    }
}

AudioLevelMeter::~AudioLevelMeter()
{
    stopTest();
}

AudioLevelMeter* AudioLevelMeter::instance()
{
    return _audioLevelMeter();
}

bool AudioLevelMeter::startTest()
{
    if (_isTesting) {
        return true;
    }

    if (!_deviceManager || !_deviceManager->isMicrophoneActive()) {
        qCWarning(AudioLevelMeterLog) << "Cannot start test: no active microphone.";
        return false;
    }

    if (!_capture) {
        return false;
    }

    const bool started = _capture->startStreaming();
    if (started) {
        _isTesting = true;
        _decayTimer->start();
        emit isTestingChanged(true);
    }
    return started;
}

void AudioLevelMeter::stopTest()
{
    if (!_isTesting) {
        return;
    }

    _isTesting = false;
    _decayTimer->stop();

    if (_capture && _capture->isStreaming()) {
        _capture->stopStreaming();
    }

    _resetLevels();
    emit isTestingChanged(false);
}

void AudioLevelMeter::toggleTest()
{
    if (_isTesting) {
        stopTest();
    } else {
        (void) startTest();
    }
}

void AudioLevelMeter::_onCaptureStateChanged()
{
    if (_isTesting && _capture && !_capture->isStreaming()) {
        _isTesting = false;
        _decayTimer->stop();
        _resetLevels();
        emit isTestingChanged(false);
    }
}

void AudioLevelMeter::_onMicrophoneActiveChanged(bool active)
{
    if (!active && _isTesting) {
        stopTest();
    }
}

void AudioLevelMeter::_onAudioStreamFrameReady(const QByteArray& pcmData, const QAudioFormat& format)
{
    if (!_isTesting || pcmData.isEmpty()) {
        return;
    }

    _processAudioBuffer(pcmData, format);
}

void AudioLevelMeter::_processAudioBuffer(const QByteArray& chunk, const QAudioFormat& format)
{
    const int sampleRate = format.sampleRate();
    if (sampleRate <= 0) {
        return;
    }

    std::vector<float> samples;
    const QAudioFormat::SampleFormat sampleFormat = format.sampleFormat();

    switch (sampleFormat) {
        case QAudioFormat::Int16: {
            const int total = chunk.size() / sizeof(qint16);
            samples.resize(total);
            const auto* src = reinterpret_cast<const qint16*>(chunk.constData());
            for (int i = 0; i < total; ++i) {
                samples[i] = std::clamp(static_cast<float>(src[i]) / 32768.0f, -1.0f, 1.0f);
            }
            break;
        }
        case QAudioFormat::Float: {
            const int total = chunk.size() / sizeof(float);
            samples.resize(total);
            const auto* src = reinterpret_cast<const float*>(chunk.constData());
            for (int i = 0; i < total; ++i) {
                samples[i] = std::clamp(src[i], -1.0f, 1.0f);
            }
            break;
        }
        case QAudioFormat::Int32: {
            const int total = chunk.size() / sizeof(qint32);
            samples.resize(total);
            const auto* src = reinterpret_cast<const qint32*>(chunk.constData());
            for (int i = 0; i < total; ++i) {
                samples[i] = std::clamp(static_cast<float>(src[i]) / 2147483648.0f, -1.0f, 1.0f);
            }
            break;
        }
        case QAudioFormat::UInt8: {
            const int total = chunk.size();
            samples.resize(total);
            const auto* src = reinterpret_cast<const quint8*>(chunk.constData());
            for (int i = 0; i < total; ++i) {
                samples[i] = std::clamp((static_cast<float>(src[i]) - 128.0f) / 128.0f, -1.0f, 1.0f);
            }
            break;
        }
        default:
            return;
    }

    const size_t N = samples.size();
    if (N == 0) {
        return;
    }

    float sumSquares = 0.0f;
    float peak = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        const float s = samples[i];
        sumSquares += s * s;
        peak = std::max(peak, std::abs(s));
    }

    const float rms = std::sqrt(sumSquares / static_cast<float>(N));
    _peakLevel = std::clamp(static_cast<qreal>(peak), 0.0, 1.0);

    if (rms > _audioLevel) {
        _audioLevel = _audioLevel * 0.25 + rms * 0.75;
    } else {
        _audioLevel = _audioLevel * 0.85;
    }
    _audioLevel = std::clamp(_audioLevel, 0.0, 1.0);

    const float fN = static_cast<float>(N);
    const float nyquist = static_cast<float>(sampleRate) / 2.0f;

    for (int b = 0; b < kDefaultBandCount; ++b) {
        const float targetFreq = kCenterFrequencies[b];
        if (targetFreq >= nyquist) {
            _frequencyBands[b] = _frequencyBands[b] * 0.85;
            continue;
        }

        const float omega = 2.0f * std::numbers::pi_v<float> * targetFreq / static_cast<float>(sampleRate);
        const float coeff = 2.0f * std::cos(omega);

        float s0 = 0.0f;
        float s1 = 0.0f;
        float s2 = 0.0f;

        for (size_t i = 0; i < N; ++i) {
            s0 = samples[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const float power = s1 * s1 + s2 * s2 - s1 * s2 * coeff;
        const float mag = std::sqrt(std::max(0.0f, power)) / fN;

        const float bandTarget = std::clamp(std::sqrt(mag * 8.0f), 0.0f, 1.0f);

        if (bandTarget > _frequencyBands[b]) {
            _frequencyBands[b] = _frequencyBands[b] * 0.25 + bandTarget * 0.75;
        } else {
            _frequencyBands[b] = _frequencyBands[b] * 0.85;
        }
        _frequencyBands[b] = std::clamp(_frequencyBands[b], 0.0, 1.0);
    }

    emit levelsChanged();
}

void AudioLevelMeter::_decayLevels()
{
    bool changed = false;

    if (_audioLevel > 0.005 || _peakLevel > 0.005) {
        _audioLevel = std::max(0.0, _audioLevel * 0.85);
        _peakLevel = std::max(0.0, _peakLevel * 0.85);
        changed = true;
    } else if (_audioLevel > 0.0 || _peakLevel > 0.0) {
        _audioLevel = 0.0;
        _peakLevel = 0.0;
        changed = true;
    }

    for (int b = 0; b < kDefaultBandCount; ++b) {
        if (_frequencyBands[b] > 0.005) {
            _frequencyBands[b] = std::max(0.0, _frequencyBands[b] * 0.85);
            changed = true;
        } else if (_frequencyBands[b] > 0.0) {
            _frequencyBands[b] = 0.0;
            changed = true;
        }
    }

    if (changed) {
        emit levelsChanged();
    }
}

void AudioLevelMeter::_resetLevels()
{
    _audioLevel = 0.0;
    _peakLevel = 0.0;
    _frequencyBands.fill(0.0);
    emit levelsChanged();
}
