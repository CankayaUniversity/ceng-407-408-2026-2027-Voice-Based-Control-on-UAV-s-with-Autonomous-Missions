#pragma once

#include <QtCore/QByteArray>
#include <QtMultimedia/QAudioFormat>

class NoiseFilterTest;

/// \brief Real-time digital signal processing (DSP) noise filter.
///
class NoiseFilter
{
    friend class NoiseFilterTest;

public:
    NoiseFilter();
    ~NoiseFilter() = default;
    NoiseFilter(const NoiseFilter&) = default;
    NoiseFilter& operator=(const NoiseFilter&) = default;
    NoiseFilter(NoiseFilter&&) = default;
    NoiseFilter& operator=(NoiseFilter&&) = default;

    /// Filter strength percentage [0, 100]. 0 = bypass, 100 = maximum suppression.
    int strength() const { return _strength; }

    void setStrength(int strengthPercent);

    /// Resets internal filter state and historical noise estimates.
    void reset();

    /// Filters raw PCM audio data in-place.
    void process(QByteArray& pcmData, const QAudioFormat& format);

private:
    void _updateHighPassCoefficients(int sampleRate);
    float _processSample(float sample);

    int _strength = 50;  // Default: 50%
    int _currentSampleRate = 0;

    // High-Pass Biquad filter coefficients (Direct Form II Transposed)
    float _b0 = 1.0f;
    float _b1 = 0.0f;
    float _b2 = 0.0f;
    float _a1 = 0.0f;
    float _a2 = 0.0f;

    // Filter state variables
    float _hpZ1 = 0.0f;
    float _hpZ2 = 0.0f;

    // Adaptive noise gate state
    float _noiseFloor = 0.01f;
    float _envelope = 0.0f;
    float _currentGain = 1.0f;
};
