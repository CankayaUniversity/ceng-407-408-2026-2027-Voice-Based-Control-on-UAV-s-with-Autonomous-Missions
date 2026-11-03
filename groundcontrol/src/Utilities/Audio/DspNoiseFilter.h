#pragma once

#include "NoiseFilterInterface.h"

class NoiseFilterTest;

/// \brief Real-time digital signal processing (DSP) noise filter.
///
class DspNoiseFilter : public NoiseFilterInterface
{
    friend class DspNoiseFilterTest;

public:
    DspNoiseFilter();
    ~DspNoiseFilter() override = default;
    DspNoiseFilter(const DspNoiseFilter&) = default;
    DspNoiseFilter& operator=(const DspNoiseFilter&) = default;
    DspNoiseFilter(DspNoiseFilter&&) = default;
    DspNoiseFilter& operator=(DspNoiseFilter&&) = default;

    Level level() const override { return _level; }

    void setLevel(Level level) override;

    void reset() override;
    void process(QByteArray& pcmData, const QAudioFormat& format) override;

    QString name() const override { return QStringLiteral("DSP"); }

private:
    void _updateHighPassCoefficients(int sampleRate);
    float _processSample(float sample);

    Level _level = Level::Moderate;
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
