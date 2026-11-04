#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSource>
#include <QtQmlIntegration/QtQmlIntegration>
#include <memory>

#include "NoiseFilterInterface.h"

class Fact;
class AudioDeviceManager;
class AudioCaptureTest;

/// \brief AudioCapture manages raw hardware audio acquisition via QAudioSource and applies noise filtering.
///
class AudioCapture : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AudioCapture)
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class AudioCaptureTest;

public:
    enum class CaptureState : uint8_t
    {
        Stopped,    ///< Hardware capture is idle / turned off.
        Streaming,  ///< Capturing and emitting live filtered audio frames to subscribers.
        Recording   ///< Capturing audio into an internal buffer.
    };
    Q_ENUM(CaptureState)

    static constexpr int kHardRecordingLimitMs = 60000;  ///< 60s memory safety ceiling

    Q_PROPERTY(CaptureState captureState READ captureState NOTIFY captureStateChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY captureStateChanged)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY captureStateChanged)
    Q_PROPERTY(bool isStreaming READ isStreaming NOTIFY captureStateChanged)
    Q_PROPERTY(
        bool noiseFilterEnabled READ isNoiseFilterEnabled WRITE setNoiseFilterEnabled NOTIFY noiseFilterEnabledChanged)
    Q_PROPERTY(
        int noiseFilterLevel READ noiseFilterLevelInt WRITE setNoiseFilterLevelInt NOTIFY noiseFilterLevelChanged)
    Q_PROPERTY(QAudioFormat actualAudioFormat READ actualAudioFormat NOTIFY audioFormatChanged)

    explicit AudioCapture(AudioDeviceManager* deviceManager = nullptr, QObject* parent = nullptr);
    ~AudioCapture() override;

    static AudioCapture* instance();

    /// Initialize with settings Facts for noise filter persistence.
    void init(Fact* noiseFilterEnabledFact = nullptr, Fact* noiseFilterLevelFact = nullptr);

    CaptureState captureState() const { return _state; }

    bool isCapturing() const { return _state != CaptureState::Stopped; }

    bool isRecording() const { return _state == CaptureState::Recording; }

    bool isStreaming() const { return _state == CaptureState::Streaming; }

    /// Noise suppression on/off state
    bool isNoiseFilterEnabled() const { return _noiseFilterEnabled; }

    void setNoiseFilterEnabled(bool enabled);

    /// Noise suppression aggressiveness level
    NoiseFilterInterface::Level noiseFilterLevel() const;
    void setNoiseFilterLevel(NoiseFilterInterface::Level level);

    int noiseFilterLevelInt() const { return static_cast<int>(noiseFilterLevel()); }

    void setNoiseFilterLevelInt(int level);

    /// Active filter instance (can be swapped with different algorithms)
    NoiseFilterInterface* noiseFilter() const { return _noiseFilter.get(); }

    void setNoiseFilter(std::unique_ptr<NoiseFilterInterface> filter);

    /// Desired format for capture (defaults to 16 kHz, 1 channel, Int16 for speech recognition).
    QAudioFormat targetAudioFormat() const { return _targetFormat; }

    void setTargetAudioFormat(const QAudioFormat& format);

    /// Actual format negotiated with the audio hardware.
    QAudioFormat actualAudioFormat() const { return _actualFormat; }

    /// Starts real-time streaming of filtered audio frames.
    Q_INVOKABLE bool startStreaming();

    /// Stops real-time streaming.
    Q_INVOKABLE void stopStreaming();

    /// Starts recording audio into internal buffer. Failsafe auto-stops at 60 seconds.
    Q_INVOKABLE bool startRecording();

    /// Stops recording and returns the recorded PCM audio buffer.
    Q_INVOKABLE QByteArray stopRecording();

    /// Stops any ongoing audio capture (streaming or recording).
    Q_INVOKABLE void stopCapture();

signals:
    void captureStateChanged(CaptureState state);
    void audioFormatChanged(const QAudioFormat& format);
    void noiseFilterEnabledChanged(bool enabled);
    void noiseFilterLevelChanged(NoiseFilterInterface::Level level);
    void captureError(const QString& errorMessage);

    /// Emitted continuously in Streaming mode for subscribers.
    void audioStreamFrameReady(const QByteArray& filteredPcmData, const QAudioFormat& format);

    /// Emitted when a recording session finishes, containing the entire PCM buffer.
    void recordingFinished(const QByteArray& recordedPcmData, const QAudioFormat& format);

    /// Emitted if recording reaches the 60-second memory safety limit and is force-stopped.
    void recordingSafetyLimitReached(const QByteArray& recordedPcmData, const QAudioFormat& format);

private slots:
    void _onReadyRead();
    void _onAudioSourceStateChanged(QAudio::State state);
    void _onDeviceAvailabilityChanged(bool active);
    void _onSelectedDeviceChanged();
    void _onSafetyLimitReached();
    void _onNoiseEnabledFactValueChanged();
    void _onNoiseLevelFactValueChanged();

private:
    bool _startCapture(CaptureState state);
    void _cleanupAudioSource();
    QAudioFormat _determineFormat(const QAudioDevice& device);

    AudioDeviceManager* _deviceManager = nullptr;
    QPointer<Fact> _noiseFilterEnabledFact;
    QPointer<Fact> _noiseFilterLevelFact;

    QAudioSource* _audioSource = nullptr;
    QIODevice* _audioIODevice = nullptr;

    CaptureState _state = CaptureState::Stopped;
    QAudioFormat _targetFormat;
    QAudioFormat _actualFormat;

    bool _noiseFilterEnabled = true;
    std::unique_ptr<NoiseFilterInterface> _noiseFilter;

    QByteArray _recordedBuffer;
    QTimer* _safetyLimitTimer = nullptr;
};
