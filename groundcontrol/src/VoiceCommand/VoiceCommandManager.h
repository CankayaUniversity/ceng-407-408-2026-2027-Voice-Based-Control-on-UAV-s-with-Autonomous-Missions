#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtMultimedia/QAudioFormat>
#include <QtQmlIntegration/QtQmlIntegration>

#include "SpeechToTextInterface.h"
#include "SttModelManager.h"

class Fact;
class AudioCapture;
class AudioDeviceManager;
class VoiceCommandManagerTest;

class VoiceCommandWorker;

/// \brief VoiceCommandManager is the central coordinator for voice command capture,
/// PTT lifecycle, background STT worker orchestration, session state machine,
/// and readiness gating.
class VoiceCommandManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VoiceCommandManager)
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class VoiceCommandManagerTest;

public:
    enum class SessionState : uint8_t
    {
        Idle = 0,
        Listening,
        Processing,
        Confirming,
        Executing,
        Error
    };
    Q_ENUM(SessionState)

    Q_PROPERTY(SessionState sessionState READ sessionState NOTIFY sessionStateChanged)
    Q_PROPERTY(QString sessionStateString READ sessionStateString NOTIFY sessionStateChanged)
    Q_PROPERTY(SttModelManager* sttModelManager READ sttModelManager CONSTANT)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(QString pushToTalkKey READ pushToTalkKey WRITE setPushToTalkKey NOTIFY pushToTalkKeyChanged)
    Q_PROPERTY(bool isMicrophoneActive READ isMicrophoneActive NOTIFY microphoneActiveChanged)
    Q_PROPERTY(bool isVehicleConnected READ isVehicleConnected NOTIFY vehicleConnectedChanged)
    Q_PROPERTY(bool isSttModelAvailable READ isSttModelAvailable NOTIFY sttModelAvailableChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY readyChanged)
    Q_PROPERTY(bool isSttModelLoaded READ isSttModelLoaded NOTIFY modelLoadedChanged)

    explicit VoiceCommandManager(AudioCapture* capture = nullptr, AudioDeviceManager* deviceManager = nullptr,
                                 SttModelManager* modelManager = nullptr, QObject* parent = nullptr);
    ~VoiceCommandManager() override;

    static VoiceCommandManager* instance();

    void init(Fact* pushToTalkKeyFact = nullptr, Fact* sttSelectedModelFact = nullptr);

    SttModelManager* sttModelManager() const { return _modelManager; }

    SessionState sessionState() const { return _sessionState; }
    QString sessionStateString() const;

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

    /// True if STT model is currently loaded in memory.
    bool isSttModelLoaded() const;

    /// Dependency injection seam for STT engine testing with mock backends.
    void setSttEngine(SpeechToTextInterface* engine, bool takeOwnership = false);
    SpeechToTextInterface* sttEngine() const;

    /// Configurable idle timeout for unloading STT models from memory (ms).
    void setIdleUnloadTimeout(int timeoutMs);
    int idleUnloadTimeout() const { return _idleUnloadTimeoutMs; }

    void setKeepModelInMemory(bool keepInMemory);
    bool keepModelInMemory() const { return _keepModelInMemory; }

    /// Starts audio recording for a voice command.
    Q_INVOKABLE bool startRecording();

    /// Stops audio recording and finishes the current voice command session.
    Q_INVOKABLE void stopRecording();

    /// Resets session to Idle.
    Q_INVOKABLE void resetSession();

signals:
    void sessionStateChanged(VoiceCommandManager::SessionState sessionState);
    void isRecordingChanged(bool isRecording);
    void pushToTalkKeyChanged(const QString& key);
    void microphoneActiveChanged(bool active);
    void vehicleConnectedChanged(bool connected);
    void sttModelAvailableChanged(bool available);
    void readyChanged(bool ready);
    void modelLoadedChanged(bool loaded);

    /// Emitted when a voice command action cannot proceed (e.g. "Voice Command is too short to recognize").
    void voiceCommandWarning(const QString& message);

    /// Emitted when a Push-to-Talk session is completed, providing the canonical audio buffer.
    void voiceCommandRecorded(const QByteArray& pcmData, const QAudioFormat& format);

    /// Emitted when speech recognition completes with non-empty text.
    void voiceCommandTranscribed(const QString& transcript, float confidence);

    /// Emitted if recording hits the memory safety ceiling (60s).
    void recordingSafetyLimitReached(const QByteArray& pcmData, const QAudioFormat& format);

    // Internal worker communication signals
    void _requestInitializeModel(const SttConfig& config);
    void _requestUnloadModel();
    void _requestTranscribeAudio(const QByteArray& pcmData, const SttConfig& config);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void _onFactValueChanged();
    void _onRecordingFinished(const QByteArray& recordedPcmData, const QAudioFormat& format);
    void _onRecordingSafetyLimitReached(const QByteArray& recordedPcmData, const QAudioFormat& format);
    void _onMicrophoneActiveChanged(bool active);
    void _onSttModelAvailableChanged(bool available);
    void _onCaptureStateChanged();

    void _onModelInitialized(bool success);
    void _onModelUnloaded();
    void _onTranscriptionFinished(const SttResult& result);
    void _onIdleTimeout();

private:
    void _setSessionState(SessionState newState);
    bool _matchesPttKey(int key, const QString& text) const;
    static bool _isTextInputFocused();

    AudioCapture* _audioCapture = nullptr;
    AudioDeviceManager* _deviceManager = nullptr;
    SttModelManager* _modelManager = nullptr;
    QPointer<Fact> _pushToTalkKeyFact;

    SessionState _sessionState = SessionState::Idle;
    QThread _workerThread;
    VoiceCommandWorker* _worker = nullptr;
    QTimer* _idleUnloadTimer = nullptr;
    int _idleUnloadTimeoutMs = 300000;
    bool _keepModelInMemory = false;
    bool _isModelLoaded = false;

    QString _pushToTalkKey = QStringLiteral("P");
    bool _isKeyPressed = false;
    bool _isEventFilterInstalled = false;
    bool _vehicleCheckEnabled = true;
    bool _sttModelCheckEnabled = true;
};
