#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtMultimedia/QAudioFormat>
#include <QtQmlIntegration/QtQmlIntegration>

#include "SttModelManager.h"

class Fact;
class AudioCapture;
class AudioDeviceManager;
class VoiceCommandManagerTest;

/// \brief VoiceCommandManager is the central coordinator for voice command capture,
/// PTT lifecycle, global key event interception, and readiness gating, delegating
/// audio capture, device management, and STT model management to sub-components.
///
class VoiceCommandManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VoiceCommandManager)
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class VoiceCommandManagerTest;

    Q_PROPERTY(SttModelManager* sttModelManager READ sttModelManager CONSTANT)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(QString pushToTalkKey READ pushToTalkKey WRITE setPushToTalkKey NOTIFY pushToTalkKeyChanged)
    Q_PROPERTY(bool isMicrophoneActive READ isMicrophoneActive NOTIFY microphoneActiveChanged)
    Q_PROPERTY(bool isVehicleConnected READ isVehicleConnected NOTIFY vehicleConnectedChanged)
    Q_PROPERTY(bool isSttModelAvailable READ isSttModelAvailable NOTIFY sttModelAvailableChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY readyChanged)

public:
    explicit VoiceCommandManager(AudioCapture* capture = nullptr, AudioDeviceManager* deviceManager = nullptr,
                                 SttModelManager* modelManager = nullptr, QObject* parent = nullptr);
    ~VoiceCommandManager() override;

    static VoiceCommandManager* instance();

    void init(Fact* pushToTalkKeyFact = nullptr, Fact* sttSelectedModelFact = nullptr,
              Fact* sttCustomModelPathFact = nullptr);

    SttModelManager* sttModelManager() const { return _modelManager; }

    bool isRecording() const;
    QString pushToTalkKey() const;
    void setPushToTalkKey(const QString& key);

    bool isMicrophoneActive() const;
    bool isVehicleConnected() const;
    void setVehicleCheckEnabled(bool enabled);

    /// True if at least one valid STT model is available via SttModelManager (or bypassed in tests).
    bool isSttModelAvailable() const;

    /// True when microphone is active, vehicle is connected, and an STT model is available.
    bool isReady() const;

    void setSttModelCheckEnabled(bool enabled);

    /// Starts audio recording for a voice command.
    Q_INVOKABLE bool startRecording();

    /// Stops audio recording and finishes the current voice command session.
    Q_INVOKABLE void stopRecording();

signals:
    void isRecordingChanged(bool isRecording);
    void pushToTalkKeyChanged(const QString& key);
    void microphoneActiveChanged(bool active);
    void vehicleConnectedChanged(bool connected);
    void sttModelAvailableChanged(bool available);
    void readyChanged(bool ready);

    /// Emitted when a voice command action cannot proceed (e.g. "Model for voice recognition is not found").
    void voiceCommandWarning(const QString& message);

    /// Emitted when a Push-to-Talk session is completed, providing the canonical audio buffer.
    void voiceCommandRecorded(const QByteArray& pcmData, const QAudioFormat& format);

    /// Emitted if recording hits the memory safety ceiling (60s).
    void recordingSafetyLimitReached(const QByteArray& pcmData, const QAudioFormat& format);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void _onFactValueChanged();
    void _onRecordingFinished(const QByteArray& recordedPcmData, const QAudioFormat& format);
    void _onRecordingSafetyLimitReached(const QByteArray& recordedPcmData, const QAudioFormat& format);
    void _onMicrophoneActiveChanged(bool active);
    void _onSttModelAvailableChanged(bool available);
    void _onCaptureStateChanged();

private:
    bool _matchesPttKey(int key, const QString& text) const;
    static bool _isTextInputFocused();

    AudioCapture* _audioCapture = nullptr;
    AudioDeviceManager* _deviceManager = nullptr;
    SttModelManager* _modelManager = nullptr;
    QPointer<Fact> _pushToTalkKeyFact;

    QString _pushToTalkKey = QStringLiteral("P");
    bool _isKeyPressed = false;
    bool _isEventFilterInstalled = false;
    bool _vehicleCheckEnabled = true;
    bool _sttModelCheckEnabled = true;
};
