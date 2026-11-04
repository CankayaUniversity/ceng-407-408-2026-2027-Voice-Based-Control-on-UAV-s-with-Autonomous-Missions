#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtMultimedia/QAudioFormat>
#include <QtQmlIntegration/QtQmlIntegration>

class AudioCapture;
class AudioDeviceManager;
class AudioLevelMeterTest;

/// \brief AudioLevelMeter computes real-time RMS, peak, and frequency band spectrum levels from incoming audio streams.
///
class AudioLevelMeter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AudioLevelMeter)
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class AudioLevelMeterTest;

public:
    static constexpr int kDefaultBandCount = 16;

    Q_PROPERTY(bool isTesting READ isTesting NOTIFY isTestingChanged)
    Q_PROPERTY(qreal audioLevel READ audioLevel NOTIFY levelsChanged)
    Q_PROPERTY(qreal peakLevel READ peakLevel NOTIFY levelsChanged)
    Q_PROPERTY(QList<qreal> frequencyBands READ frequencyBands NOTIFY levelsChanged)
    Q_PROPERTY(int bandCount READ bandCount CONSTANT)

    explicit AudioLevelMeter(AudioCapture* capture = nullptr, AudioDeviceManager* deviceManager = nullptr,
                             QObject* parent = nullptr);
    ~AudioLevelMeter() override;

    static AudioLevelMeter* instance();

    bool isTesting() const { return _isTesting; }

    qreal audioLevel() const { return _audioLevel; }

    qreal peakLevel() const { return _peakLevel; }

    QList<qreal> frequencyBands() const { return _frequencyBands; }

    int bandCount() const { return kDefaultBandCount; }

    /// Starts streaming capture and begins level analysis.
    Q_INVOKABLE bool startTest();

    /// Stops streaming capture and clears levels.
    Q_INVOKABLE void stopTest();

    /// Toggles the test state.
    Q_INVOKABLE void toggleTest();

signals:
    void isTestingChanged(bool isTesting);
    void levelsChanged();

private slots:
    void _onAudioStreamFrameReady(const QByteArray& pcmData, const QAudioFormat& format);
    void _onCaptureStateChanged();
    void _onMicrophoneActiveChanged(bool active);
    void _decayLevels();

private:
    void _processAudioBuffer(const QByteArray& chunk, const QAudioFormat& format);
    void _resetLevels();

    AudioCapture* _capture = nullptr;
    AudioDeviceManager* _deviceManager = nullptr;

    bool _isTesting = false;
    qreal _audioLevel = 0.0;
    qreal _peakLevel = 0.0;
    QList<qreal> _frequencyBands;

    QTimer* _decayTimer = nullptr;

    static constexpr float kCenterFrequencies[kDefaultBandCount] = {
        100.0f,  140.0f,  200.0f,  280.0f,  400.0f,  560.0f,  750.0f,  1000.0f,
        1350.0f, 1800.0f, 2400.0f, 3200.0f, 4000.0f, 4800.0f, 5800.0f, 6800.0f};
    static constexpr float kBandBoost[kDefaultBandCount] = {1.8f, 1.5f, 1.3f, 1.2f, 1.2f, 1.3f, 1.4f, 1.6f,
                                                            1.8f, 2.0f, 2.4f, 2.8f, 3.2f, 3.6f, 4.0f, 4.5f};
};
