#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtMultimedia/QAudioFormat>
#include <QtQmlIntegration/QtQmlIntegration>

class Fact;
class AudioCapture;
class AudioDeviceManager;
class VoiceCommandManagerTest;

/// \brief VoiceCommandManager is the central coordinator for voice command capture,
/// PTT lifecycle, global key event interception, and downstream speech recognition.
///
class VoiceCommandManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VoiceCommandManager)
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class VoiceCommandManagerTest;

    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(QString pushToTalkKey READ pushToTalkKey WRITE setPushToTalkKey NOTIFY pushToTalkKeyChanged)
    Q_PROPERTY(bool isMicrophoneActive READ isMicrophoneActive NOTIFY microphoneActiveChanged)

public:
    explicit VoiceCommandManager(AudioCapture* capture = nullptr, AudioDeviceManager* deviceManager = nullptr,
                                 QObject* parent = nullptr);
    ~VoiceCommandManager() override;

    static VoiceCommandManager* instance();

    void init(Fact* pushToTalkKeyFact = nullptr);

    bool isRecording() const;
    QString pushToTalkKey() const;
    void setPushToTalkKey(const QString& key);

    bool isMicrophoneActive() const;

    /// Starts audio recording for a voice command.
    Q_INVOKABLE bool startRecording();

    /// Stops audio recording and finishes the current voice command session.
    Q_INVOKABLE void stopRecording();

signals:
    void isRecordingChanged(bool isRecording);
    void pushToTalkKeyChanged(const QString& key);
    void microphoneActiveChanged(bool active);

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
    void _onCaptureStateChanged();

private:
    bool _matchesPttKey(int key, const QString& text) const;
    static bool _isTextInputFocused();

    AudioCapture* _audioCapture = nullptr;
    AudioDeviceManager* _deviceManager = nullptr;
    QPointer<Fact> _pushToTalkKeyFact;

    QString _pushToTalkKey = QStringLiteral("P");
    bool _isKeyPressed = false;
    bool _isEventFilterInstalled = false;
};
