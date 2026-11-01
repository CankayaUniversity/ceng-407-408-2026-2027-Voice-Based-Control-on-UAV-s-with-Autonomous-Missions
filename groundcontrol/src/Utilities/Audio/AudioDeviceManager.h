#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaDevices>
#include <QtQmlIntegration/QtQmlIntegration>

class Fact;
class AudioDeviceManagerTest;

/// \brief AudioDeviceManager manages microphone input devices, tracks availability and specs.
///
class AudioDeviceManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class AudioDeviceManagerTest;

    Q_PROPERTY(QStringList availableMicrophones READ availableMicrophones NOTIFY availableMicrophonesChanged)
    Q_PROPERTY(
        QString selectedMicrophone READ selectedMicrophone WRITE setSelectedMicrophone NOTIFY selectedMicrophoneChanged)
    Q_PROPERTY(int selectedMicrophoneIndex READ selectedMicrophoneIndex WRITE setSelectedMicrophoneIndex NOTIFY
                   selectedMicrophoneIndexChanged)
    Q_PROPERTY(bool isMicrophoneActive READ isMicrophoneActive NOTIFY isMicrophoneActiveChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY specsChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY specsChanged)
    Q_PROPERTY(QString sampleFormat READ sampleFormat NOTIFY specsChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY specsChanged)
    Q_PROPERTY(bool isDefaultDevice READ isDefaultDevice NOTIFY specsChanged)

public:
    explicit AudioDeviceManager(QObject* parent = nullptr);
    ~AudioDeviceManager() override;

    static AudioDeviceManager* instance();

    /// Initialize with settings Fact for persistence.
    void init(Fact* selectedMicrophoneFact = nullptr);

    /// List of human-readable descriptions of available microphones.
    QStringList availableMicrophones() const;

    /// Currently selected microphone description.
    QString selectedMicrophone() const;
    void setSelectedMicrophone(const QString& microphone);

    /// Index of selected microphone in availableMicrophones list (-1 if none/invalid).
    int selectedMicrophoneIndex() const;
    void setSelectedMicrophoneIndex(int index);

    /// True if the selected microphone is valid, connected, and available for capture.
    bool isMicrophoneActive() const;

    /// Audio hardware specifications of the currently active microphone.
    int sampleRate() const;
    int channelCount() const;
    QString sampleFormat() const;
    QString deviceId() const;
    bool isDefaultDevice() const;

    /// Returns the explicit QAudioDevice instance for capture modules.
    QAudioDevice selectedAudioDevice() const;

    /// Returns the preferred format of the selected device.
    QAudioFormat preferredAudioFormat() const;

signals:
    void availableMicrophonesChanged();
    void selectedMicrophoneChanged(const QString& microphone);
    void selectedMicrophoneIndexChanged(int index);
    void isMicrophoneActiveChanged(bool active);
    void specsChanged();

private slots:
    void _updateAvailableDevices();
    void _onFactValueChanged();

private:
    void _updateSelectedDevice();
    static QString _sampleFormatToString(QAudioFormat::SampleFormat format);

    QMediaDevices* _mediaDevices = nullptr;
    QPointer<Fact> _selectedMicrophoneFact;

    QString _selectedMicrophoneName;
    QList<QAudioDevice> _audioInputDevices;
    QAudioDevice _selectedDevice;
    bool _isMicrophoneActive = false;
};