#include "AudioDeviceManager.h"

#include "Fact.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AudioDeviceManagerLog, "Utilities.AudioDeviceManager")

Q_APPLICATION_STATIC(AudioDeviceManager, _audioDeviceManager);

AudioDeviceManager::AudioDeviceManager(QObject* parent) : QObject(parent), _mediaDevices(new QMediaDevices(this))
{
    (void) connect(_mediaDevices, &QMediaDevices::audioInputsChanged, this,
                   &AudioDeviceManager::_updateAvailableDevices);
    _updateAvailableDevices();
}

AudioDeviceManager::~AudioDeviceManager() {}

AudioDeviceManager* AudioDeviceManager::instance()
{
    return _audioDeviceManager();
}

void AudioDeviceManager::init(Fact* selectedMicrophoneFact)
{
    if (_selectedMicrophoneFact == selectedMicrophoneFact) {
        return;
    }

    if (_selectedMicrophoneFact) {
        (void) disconnect(_selectedMicrophoneFact, &Fact::rawValueChanged, this,
                          &AudioDeviceManager::_onFactValueChanged);
    }

    _selectedMicrophoneFact = selectedMicrophoneFact;

    if (_selectedMicrophoneFact) {
        (void) connect(_selectedMicrophoneFact, &Fact::rawValueChanged, this, &AudioDeviceManager::_onFactValueChanged);
        const QString savedName = _selectedMicrophoneFact->rawValue().toString();
        if (!savedName.isEmpty()) {
            _selectedMicrophoneName = savedName;
        }
    }

    _updateSelectedDevice();
}

QStringList AudioDeviceManager::availableMicrophones() const
{
    QStringList names;
    for (const QAudioDevice& device : _audioInputDevices) {
        names.append(device.description());
    }
    return names;
}

QString AudioDeviceManager::selectedMicrophone() const
{
    return _selectedMicrophoneName;
}

void AudioDeviceManager::setSelectedMicrophone(const QString& microphone)
{
    if (_selectedMicrophoneName == microphone) {
        return;
    }

    _selectedMicrophoneName = microphone;

    if (_selectedMicrophoneFact && _selectedMicrophoneFact->rawValue().toString() != microphone) {
        _selectedMicrophoneFact->setRawValue(microphone);
    }

    _updateSelectedDevice();
    emit selectedMicrophoneChanged(_selectedMicrophoneName);
    emit selectedMicrophoneIndexChanged(selectedMicrophoneIndex());
}

int AudioDeviceManager::selectedMicrophoneIndex() const
{
    if (_selectedMicrophoneName.isEmpty()) {
        return -1;
    }
    return availableMicrophones().indexOf(_selectedMicrophoneName);
}

void AudioDeviceManager::setSelectedMicrophoneIndex(int index)
{
    const QStringList names = availableMicrophones();
    if (index >= 0 && index < names.count()) {
        setSelectedMicrophone(names.at(index));
    }
}

bool AudioDeviceManager::isMicrophoneActive() const
{
    return _isMicrophoneActive;
}

int AudioDeviceManager::sampleRate() const
{
    return _isMicrophoneActive ? _selectedDevice.preferredFormat().sampleRate() : 0;
}

int AudioDeviceManager::channelCount() const
{
    return _isMicrophoneActive ? _selectedDevice.preferredFormat().channelCount() : 0;
}

QString AudioDeviceManager::sampleFormat() const
{
    return _isMicrophoneActive ? _sampleFormatToString(_selectedDevice.preferredFormat().sampleFormat()) : QString();
}

QString AudioDeviceManager::deviceId() const
{
    return _isMicrophoneActive ? QString::fromUtf8(_selectedDevice.id()) : QString();
}

bool AudioDeviceManager::isDefaultDevice() const
{
    return _isMicrophoneActive && _selectedDevice.isDefault();
}

QAudioDevice AudioDeviceManager::selectedAudioDevice() const
{
    return _isMicrophoneActive ? _selectedDevice : QAudioDevice();
}

QAudioFormat AudioDeviceManager::preferredAudioFormat() const
{
    return _isMicrophoneActive ? _selectedDevice.preferredFormat() : QAudioFormat();
}

void AudioDeviceManager::_updateAvailableDevices()
{
    _audioInputDevices = QMediaDevices::audioInputs();
    qCDebug(AudioDeviceManagerLog) << "Available microphones updated, count:" << _audioInputDevices.count();

    _updateSelectedDevice();
    emit availableMicrophonesChanged();
    emit selectedMicrophoneIndexChanged(selectedMicrophoneIndex());
}

void AudioDeviceManager::_onFactValueChanged()
{
    if (!_selectedMicrophoneFact) {
        return;
    }

    const QString factName = _selectedMicrophoneFact->rawValue().toString();
    if (_selectedMicrophoneName != factName) {
        _selectedMicrophoneName = factName;
        _updateSelectedDevice();
        emit selectedMicrophoneChanged(_selectedMicrophoneName);
        emit selectedMicrophoneIndexChanged(selectedMicrophoneIndex());
    }
}

void AudioDeviceManager::_updateSelectedDevice()
{
    QAudioDevice matchedDevice;

    if (!_selectedMicrophoneName.isEmpty()) {
        for (const QAudioDevice& device : _audioInputDevices) {
            if (device.description() == _selectedMicrophoneName) {
                matchedDevice = device;
                break;
            }
        }
    } else if (!_audioInputDevices.isEmpty()) {
        const QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
        if (!defaultDevice.isNull()) {
            for (const QAudioDevice& device : _audioInputDevices) {
                if (device.id() == defaultDevice.id() || device.description() == defaultDevice.description()) {
                    matchedDevice = device;
                    break;
                }
            }
        }
        if (matchedDevice.isNull()) {
            matchedDevice = _audioInputDevices.first();
        }

        _selectedMicrophoneName = matchedDevice.description();
        if (_selectedMicrophoneFact) {
            _selectedMicrophoneFact->setRawValue(_selectedMicrophoneName);
        }
        emit selectedMicrophoneChanged(_selectedMicrophoneName);
    }

    const bool wasActive = _isMicrophoneActive;
    _selectedDevice = matchedDevice;
    _isMicrophoneActive = !_selectedDevice.isNull();

    qCDebug(AudioDeviceManagerLog) << "Selected device:" << _selectedMicrophoneName << "active:" << _isMicrophoneActive;

    if (wasActive != _isMicrophoneActive) {
        emit isMicrophoneActiveChanged(_isMicrophoneActive);
    }
    emit specsChanged();
}

QString AudioDeviceManager::_sampleFormatToString(QAudioFormat::SampleFormat format)
{
    switch (format) {
        case QAudioFormat::UInt8:
            return QStringLiteral("UInt8");
        case QAudioFormat::Int16:
            return QStringLiteral("Int16");
        case QAudioFormat::Int32:
            return QStringLiteral("Int32");
        case QAudioFormat::Float:
            return QStringLiteral("Float32");
        case QAudioFormat::Unknown:
        default:
            return QStringLiteral("Unknown");
    }
}