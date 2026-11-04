#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtMultimedia/QAudioFormat>

/// \brief Abstract interface for real-time audio noise suppression algorithms.
///
class NoiseFilterInterface
{
public:
    enum class Level
    {
        Low = 0,       ///< Mild suppression (indoor / quiet environments)
        Moderate = 1,  ///< Balanced suppression (normal outdoor environments)
        High = 2       ///< Aggressive suppression (noisy UAV field / engine drone)
    };

    virtual ~NoiseFilterInterface() = default;

    /// Current suppression aggressiveness level
    virtual Level level() const = 0;

    /// Updates suppression aggressiveness level
    virtual void setLevel(Level level) = 0;

    /// Resets internal filter state, filters buffers and historical noise estimates
    virtual void reset() = 0;

    /// Filters raw PCM audio data in-place
    virtual void process(QByteArray& pcmData, const QAudioFormat& format) = 0;

    /// Algorithm name
    virtual QString name() const = 0;
};
