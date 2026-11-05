#pragma once

#include <QtCore/QByteArray>
#include <QtMultimedia/QAudioFormat>
#include <vector>

/// \brief AudioDownsampler downmixes arbitrary multi-channel audio formats to mono,
/// applies an anti-aliasing low-pass filter, and resamples streaming chunks to canonical
/// 16 kHz Mono Int16 PCM
///
class AudioDownsampler
{
public:
    static constexpr int kCanonicalSampleRate = 16000;
    static constexpr int kCanonicalChannelCount = 1;

    AudioDownsampler();
    ~AudioDownsampler() = default;
    AudioDownsampler(const AudioDownsampler&) = default;
    AudioDownsampler& operator=(const AudioDownsampler&) = default;
    AudioDownsampler(AudioDownsampler&&) = default;
    AudioDownsampler& operator=(AudioDownsampler&&) = default;

    /// Resets internal anti-aliasing filter and fractional resampling phase between capture sessions.
    void reset();

    /// Ingestion boundary processor: downmixes multi-channel frames to mono, resamples to 16 kHz,
    /// and converts to signed 16-bit integer PCM.
    QByteArray process(const QByteArray& rawBytes, const QAudioFormat& sourceFormat);

    /// Convenience helper returning canonical 16 kHz Mono Int16 audio format.
    static QAudioFormat canonicalFormat();

private:
    struct AntiAliasingFilter
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
        int currentRate = 0;

        void update(int sampleRate);
        void reset();
        float process(float in);
    };

    std::vector<float> _downmixToMonoFloat(const QByteArray& rawBytes, const QAudioFormat& format);
    std::vector<float> _resampleTo16kHz(const std::vector<float>& inputMono, int inSampleRate);
    static QByteArray _floatToCanonicalInt16Pcm(const std::vector<float>& samples);

    AntiAliasingFilter _antiAliasingFilter;
    double _resamplePos = 0.0;
    float _carryoverSample = 0.0f;
    bool _hasCarryover = false;
};
