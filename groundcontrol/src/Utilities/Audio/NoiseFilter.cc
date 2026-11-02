#include "NoiseFilter.h"

#include <algorithm>
#include <cmath>

NoiseFilter::NoiseFilter()
{
    reset();
}

void NoiseFilter::setStrength(int strengthPercent)
{
    _strength = std::clamp(strengthPercent, 0, 100);
}

void NoiseFilter::reset()
{
    _hpZ1 = 0.0f;
    _hpZ2 = 0.0f;
    _noiseFloor = 0.01f;
    _envelope = 0.0f;
    _currentGain = 1.0f;
    _currentSampleRate = 0;
}

void NoiseFilter::_updateHighPassCoefficients(int sampleRate)
{
    if (sampleRate <= 0 || sampleRate == _currentSampleRate) {
        return;
    }

    _currentSampleRate = sampleRate;

    constexpr float cutoffHz = 80.0f;
    constexpr float q = 0.70710678f;,

    const float omega = 2.0f * static_cast<float>(M_PI) * cutoffHz / static_cast<float>(sampleRate);
    const float cosOmega = std::cos(omega);
    const float sinOmega = std::sin(omega);
    const float alpha = sinOmega / (2.0f * q);

    const float a0 = 1.0f + alpha;
    _b0 = ((1.0f + cosOmega) / 2.0f) / a0;
    _b1 = (-(1.0f + cosOmega)) / a0;
    _b2 = ((1.0f + cosOmega) / 2.0f) / a0;
    _a1 = (-2.0f * cosOmega) / a0;
    _a2 = (1.0f - alpha) / a0;
}

float NoiseFilter::_processSample(float sample)
{
    const float hpOut = _b0 * sample + _hpZ1;
    _hpZ1 = _b1 * sample - _a1 * hpOut + _hpZ2;
    _hpZ2 = _b2 * sample - _a2 * hpOut;

    const float absVal = std::abs(hpOut);

    if (absVal > _envelope) {
        _envelope = _envelope * 0.85f + absVal * 0.15f;
    } else {
        _envelope = _envelope * 0.995f + absVal * 0.005f;
    }

    if (_envelope < _noiseFloor) {
        _noiseFloor = _noiseFloor * 0.99f + _envelope * 0.01f;
    } else {
        _noiseFloor = _noiseFloor * 0.9998f + _envelope * 0.0002f;
    }
    _noiseFloor = std::clamp(_noiseFloor, 0.0005f, 0.5f);

    const float threshold = _noiseFloor * (1.5f + 1.5f * (static_cast<float>(_strength) / 100.0f));
    const float minGain = 1.0f - (static_cast<float>(_strength) / 100.0f) * 0.95f;  // 0% -> 1.0, 100% -> 0.05

    float targetGain = 1.0f;
    if (_envelope < threshold) {
        const float ratio = std::clamp(_envelope / threshold, 0.0f, 1.0f);
        targetGain = minGain + (1.0f - minGain) * ratio;
    }

    _currentGain = _currentGain * 0.97f + targetGain * 0.03f;

    return std::clamp(hpOut * _currentGain, -1.0f, 1.0f);
}

void NoiseFilter::process(QByteArray& pcmData, const QAudioFormat& format)
{
    if (_strength == 0 || pcmData.isEmpty()) {
        return;
    }

    _updateHighPassCoefficients(format.sampleRate());

    switch (format.sampleFormat()) {
        case QAudioFormat::Int16: {
            const int totalSamples = pcmData.size() / sizeof(qint16);
            auto* samples = reinterpret_cast<qint16*>(pcmData.data());
            for (int i = 0; i < totalSamples; ++i) {
                const float inVal = static_cast<float>(samples[i]) / 32768.0f;
                const float outVal = _processSample(inVal);
                samples[i] = static_cast<qint16>(std::clamp(outVal * 32767.0f, -32768.0f, 32767.0f));
            }
            break;
        }
        case QAudioFormat::Float: {
            const int totalSamples = pcmData.size() / sizeof(float);
            auto* samples = reinterpret_cast<float*>(pcmData.data());
            for (int i = 0; i < totalSamples; ++i) {
                samples[i] = _processSample(samples[i]);
            }
            break;
        }
        case QAudioFormat::Int32: {
            const int totalSamples = pcmData.size() / sizeof(qint32);
            auto* samples = reinterpret_cast<qint32*>(pcmData.data());
            for (int i = 0; i < totalSamples; ++i) {
                const float inVal = static_cast<float>(samples[i]) / 2147483648.0f;
                const float outVal = _processSample(inVal);
                samples[i] = static_cast<qint32>(std::clamp(outVal * 2147483647.0f, -2147483648.0f, 2147483647.0f));
            }
            break;
        }
        case QAudioFormat::UInt8: {
            const int totalSamples = pcmData.size();
            auto* samples = reinterpret_cast<quint8*>(pcmData.data());
            for (int i = 0; i < totalSamples; ++i) {
                const float inVal = (static_cast<float>(samples[i]) - 128.0f) / 128.0f;
                const float outVal = _processSample(inVal);
                samples[i] = static_cast<quint8>(std::clamp(outVal * 127.0f + 128.0f, 0.0f, 255.0f));
            }
            break;
        }
        default:
            break;
    }
}
