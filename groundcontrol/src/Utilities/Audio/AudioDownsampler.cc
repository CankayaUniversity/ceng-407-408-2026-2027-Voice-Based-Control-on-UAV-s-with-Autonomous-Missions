#include "AudioDownsampler.h"

#include <algorithm>
#include <cmath>

AudioDownsampler::AudioDownsampler()
{
    reset();
}

void AudioDownsampler::reset()
{
    _antiAliasingFilter.reset();
    _resamplePos = 0.0;
    _carryoverSample = 0.0f;
    _hasCarryover = false;
}

QAudioFormat AudioDownsampler::canonicalFormat()
{
    QAudioFormat format;
    format.setSampleRate(kCanonicalSampleRate);
    format.setChannelCount(kCanonicalChannelCount);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

void AudioDownsampler::AntiAliasingFilter::update(int sampleRate)
{
    if (sampleRate <= kCanonicalSampleRate || sampleRate == currentRate) {
        return;
    }
    currentRate = sampleRate;

    // 2nd-order Butterworth low-pass filter with 7.5 kHz cutoff (below 8 kHz Nyquist)
    constexpr float cutoff = 7500.0f;
    constexpr float q = 0.70710678f;
    const float omega = 2.0f * static_cast<float>(M_PI) * cutoff / static_cast<float>(sampleRate);
    const float cosOmega = std::cos(omega);
    const float sinOmega = std::sin(omega);
    const float alpha = sinOmega / (2.0f * q);

    const float a0 = 1.0f + alpha;
    b0 = ((1.0f - cosOmega) / 2.0f) / a0;
    b1 = (1.0f - cosOmega) / a0;
    b2 = ((1.0f - cosOmega) / 2.0f) / a0;
    a1 = (-2.0f * cosOmega) / a0;
    a2 = (1.0f - alpha) / a0;
}

void AudioDownsampler::AntiAliasingFilter::reset()
{
    z1 = 0.0f;
    z2 = 0.0f;
    currentRate = 0;
}

float AudioDownsampler::AntiAliasingFilter::process(float in)
{
    if (currentRate <= kCanonicalSampleRate) {
        return in;
    }
    const float out = b0 * in + z1;
    z1 = b1 * in - a1 * out + z2;
    z2 = b2 * in - a2 * out;
    return out;
}

std::vector<float> AudioDownsampler::_downmixToMonoFloat(const QByteArray& rawBytes, const QAudioFormat& format)
{
    if (rawBytes.isEmpty() || !format.isValid() || format.sampleRate() <= 0 || format.channelCount() <= 0) {
        return {};
    }

    const int channels = format.channelCount();
    const QAudioFormat::SampleFormat sampleFormat = format.sampleFormat();
    std::vector<float> mono;

    switch (sampleFormat) {
        case QAudioFormat::Int16: {
            const auto* src = reinterpret_cast<const qint16*>(rawBytes.constData());
            const int totalSamples = static_cast<int>(rawBytes.size() / sizeof(qint16));
            const int numFrames = totalSamples / channels;
            mono.resize(numFrames);

            if (channels == 1) {
                for (int i = 0; i < numFrames; ++i) {
                    mono[i] = static_cast<float>(src[i]) / 32768.0f;
                }
            } else {
                for (int i = 0; i < numFrames; ++i) {
                    int32_t sum = 0;
                    const int offset = i * channels;
                    for (int c = 0; c < channels; ++c) {
                        sum += src[offset + c];
                    }
                    mono[i] = static_cast<float>(sum) / (static_cast<float>(channels) * 32768.0f);
                }
            }
            break;
        }
        case QAudioFormat::Float: {
            const auto* src = reinterpret_cast<const float*>(rawBytes.constData());
            const int totalSamples = static_cast<int>(rawBytes.size() / sizeof(float));
            const int numFrames = totalSamples / channels;
            mono.resize(numFrames);

            if (channels == 1) {
                for (int i = 0; i < numFrames; ++i) {
                    mono[i] = src[i];
                }
            } else {
                for (int i = 0; i < numFrames; ++i) {
                    float sum = 0.0f;
                    const int offset = i * channels;
                    for (int c = 0; c < channels; ++c) {
                        sum += src[offset + c];
                    }
                    mono[i] = sum / static_cast<float>(channels);
                }
            }
            break;
        }
        case QAudioFormat::Int32: {
            const auto* src = reinterpret_cast<const qint32*>(rawBytes.constData());
            const int totalSamples = static_cast<int>(rawBytes.size() / sizeof(qint32));
            const int numFrames = totalSamples / channels;
            mono.resize(numFrames);

            if (channels == 1) {
                for (int i = 0; i < numFrames; ++i) {
                    mono[i] = static_cast<float>(src[i]) / 2147483648.0f;
                }
            } else {
                for (int i = 0; i < numFrames; ++i) {
                    int64_t sum = 0;
                    const int offset = i * channels;
                    for (int c = 0; c < channels; ++c) {
                        sum += src[offset + c];
                    }
                    mono[i] = static_cast<float>(sum) / (static_cast<float>(channels) * 2147483648.0f);
                }
            }
            break;
        }
        case QAudioFormat::UInt8: {
            const auto* src = reinterpret_cast<const quint8*>(rawBytes.constData());
            const int totalSamples = static_cast<int>(rawBytes.size() / sizeof(quint8));
            const int numFrames = totalSamples / channels;
            mono.resize(numFrames);

            if (channels == 1) {
                for (int i = 0; i < numFrames; ++i) {
                    mono[i] = (static_cast<float>(src[i]) - 128.0f) / 128.0f;
                }
            } else {
                for (int i = 0; i < numFrames; ++i) {
                    int32_t sum = 0;
                    const int offset = i * channels;
                    for (int c = 0; c < channels; ++c) {
                        sum += static_cast<int32_t>(src[offset + c]) - 128;
                    }
                    mono[i] = static_cast<float>(sum) / (static_cast<float>(channels) * 128.0f);
                }
            }
            break;
        }
        default:
            break;
    }

    return mono;
}

std::vector<float> AudioDownsampler::_resampleTo16kHz(const std::vector<float>& inputMono, int inSampleRate)
{
    if (inputMono.empty() || inSampleRate <= 0) {
        return {};
    }

    if (inSampleRate == kCanonicalSampleRate) {
        return inputMono;
    }

    _antiAliasingFilter.update(inSampleRate);

    std::vector<float> filtered(inputMono.size());
    for (size_t i = 0; i < inputMono.size(); ++i) {
        filtered[i] = _antiAliasingFilter.process(inputMono[i]);
    }

    std::vector<float> buffer;
    if (_hasCarryover) {
        buffer.reserve(filtered.size() + 1);
        buffer.push_back(_carryoverSample);
        buffer.insert(buffer.end(), filtered.begin(), filtered.end());
    } else {
        buffer = std::move(filtered);
    }

    const int total = static_cast<int>(buffer.size());
    if (total < 2) {
        _carryoverSample = buffer.back();
        _hasCarryover = true;
        return {};
    }

    const double step = static_cast<double>(inSampleRate) / static_cast<double>(kCanonicalSampleRate);
    std::vector<float> output;
    output.reserve(static_cast<size_t>(std::ceil(total / step) + 2));

    while (_resamplePos < static_cast<double>(total - 1)) {
        const int k = static_cast<int>(_resamplePos);
        const double frac = _resamplePos - static_cast<double>(k);
        const float s0 = buffer[k];
        const float s1 = buffer[k + 1];
        const float interpolated = s0 + static_cast<float>(frac * (s1 - s0));
        output.push_back(interpolated);

        _resamplePos += step;
    }

    _resamplePos -= static_cast<double>(total - 1);
    _carryoverSample = buffer.back();
    _hasCarryover = true;

    return output;
}

QByteArray AudioDownsampler::_floatToCanonicalInt16Pcm(const std::vector<float>& samples)
{
    if (samples.empty()) {
        return QByteArray();
    }

    QByteArray pcm;
    pcm.resize(static_cast<qsizetype>(samples.size() * sizeof(qint16)));
    auto* dst = reinterpret_cast<qint16*>(pcm.data());

    for (size_t i = 0; i < samples.size(); ++i) {
        const float clamped = std::clamp(samples[i], -1.0f, 1.0f);
        dst[i] = static_cast<qint16>(std::clamp(std::round(clamped * 32767.0f), -32768.0f, 32767.0f));
    }

    return pcm;
}

QByteArray AudioDownsampler::process(const QByteArray& rawBytes, const QAudioFormat& sourceFormat)
{
    if (rawBytes.isEmpty()) {
        return QByteArray();
    }

    const std::vector<float> mono = _downmixToMonoFloat(rawBytes, sourceFormat);
    if (mono.empty()) {
        return QByteArray();
    }

    const std::vector<float> resampled = _resampleTo16kHz(mono, sourceFormat.sampleRate());
    if (resampled.empty()) {
        return QByteArray();
    }

    return _floatToCanonicalInt16Pcm(resampled);
}
