#include "AudioCapture.h"

#include <algorithm>

#include "AudioDeviceManager.h"
#include "Fact.h"
#include "DspNoiseFilter.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AudioCaptureLog, "Utilities.AudioCapture")

Q_APPLICATION_STATIC(AudioCapture, _audioCapture);

AudioCapture::AudioCapture(AudioDeviceManager* deviceManager, QObject* parent)
    : QObject(parent),
      _deviceManager(deviceManager ? deviceManager : AudioDeviceManager::instance()),
      _noiseFilter(std::make_unique<DspNoiseFilter>()),
      _safetyLimitTimer(new QTimer(this))
{
    _targetFormat.setSampleRate(16000);
    _targetFormat.setChannelCount(1);
    _targetFormat.setSampleFormat(QAudioFormat::Int16);

    _safetyLimitTimer->setSingleShot(true);
    (void) connect(_safetyLimitTimer, &QTimer::timeout, this, &AudioCapture::_onSafetyLimitReached);

    if (_deviceManager != nullptr) {
        (void) connect(_deviceManager, &AudioDeviceManager::isMicrophoneActiveChanged, this,
                       &AudioCapture::_onDeviceAvailabilityChanged);
        (void) connect(_deviceManager, &AudioDeviceManager::selectedMicrophoneChanged, this,
                       &AudioCapture::_onSelectedDeviceChanged);
    }
}

AudioCapture::~AudioCapture()
{
    stopCapture();
}

AudioCapture* AudioCapture::instance()
{
    return _audioCapture();
}

void AudioCapture::init(Fact* noiseFilterEnabledFact, Fact* noiseFilterLevelFact)
{
    if (_noiseFilterEnabledFact != noiseFilterEnabledFact) {
        if (_noiseFilterEnabledFact) {
            (void) disconnect(_noiseFilterEnabledFact, &Fact::rawValueChanged, this,
                              &AudioCapture::_onNoiseEnabledFactValueChanged);
        }
        _noiseFilterEnabledFact = noiseFilterEnabledFact;
        if (_noiseFilterEnabledFact) {
            (void) connect(_noiseFilterEnabledFact, &Fact::rawValueChanged, this,
                           &AudioCapture::_onNoiseEnabledFactValueChanged);
            _onNoiseEnabledFactValueChanged();
        }
    }

    if (_noiseFilterLevelFact != noiseFilterLevelFact) {
        if (_noiseFilterLevelFact) {
            (void) disconnect(_noiseFilterLevelFact, &Fact::rawValueChanged, this,
                              &AudioCapture::_onNoiseLevelFactValueChanged);
        }
        _noiseFilterLevelFact = noiseFilterLevelFact;
        if (_noiseFilterLevelFact) {
            (void) connect(_noiseFilterLevelFact, &Fact::rawValueChanged, this,
                           &AudioCapture::_onNoiseLevelFactValueChanged);
            _onNoiseLevelFactValueChanged();
        }
    }
}

void AudioCapture::_onNoiseEnabledFactValueChanged()
{
    if (!_noiseFilterEnabledFact) {
        return;
    }

    const bool enabled = _noiseFilterEnabledFact->rawValue().toBool();
    if (_noiseFilterEnabled != enabled) {
        setNoiseFilterEnabled(enabled);
    }
}

void AudioCapture::_onNoiseLevelFactValueChanged()
{
    if (!_noiseFilterLevelFact) {
        return;
    }

    const int levelInt = _noiseFilterLevelFact->rawValue().toInt();
    const auto level = static_cast<NoiseFilterInterface::Level>(std::clamp(levelInt, 0, 2));
    if (noiseFilterLevel() != level) {
        setNoiseFilterLevel(level);
    }
}

void AudioCapture::setNoiseFilterEnabled(bool enabled)
{
    if (_noiseFilterEnabled == enabled) {
        return;
    }

    _noiseFilterEnabled = enabled;

    if (_noiseFilterEnabledFact && _noiseFilterEnabledFact->rawValue().toBool() != enabled) {
        _noiseFilterEnabledFact->setRawValue(enabled);
    }

    emit noiseFilterEnabledChanged(enabled);
}

NoiseFilterInterface::Level AudioCapture::noiseFilterLevel() const
{
    return _noiseFilter ? _noiseFilter->level() : NoiseFilterInterface::Level::Moderate;
}

void AudioCapture::setNoiseFilterLevel(NoiseFilterInterface::Level level)
{
    if (_noiseFilter) {
        if (_noiseFilter->level() == level) {
            return;
        }
        _noiseFilter->setLevel(level);
    }

    if (_noiseFilterLevelFact && _noiseFilterLevelFact->rawValue().toInt() != static_cast<int>(level)) {
        _noiseFilterLevelFact->setRawValue(static_cast<int>(level));
    }

    emit noiseFilterLevelChanged(level);
}

void AudioCapture::setNoiseFilterLevelInt(int level)
{
    const int clamped = std::clamp(level, 0, 2);
    setNoiseFilterLevel(static_cast<NoiseFilterInterface::Level>(clamped));
}

void AudioCapture::setNoiseFilter(std::unique_ptr<NoiseFilterInterface> filter)
{
    _noiseFilter = std::move(filter);
    if (_noiseFilter) {
        _noiseFilter->reset();
    }
}

void AudioCapture::setTargetAudioFormat(const QAudioFormat& format)
{
    if (_targetFormat == format) {
        return;
    }

    _targetFormat = format;

    if (_state != CaptureState::Stopped) {
        const CaptureState currentState = _state;
        stopCapture();
        _startCapture(currentState);
    }
}

bool AudioCapture::startStreaming()
{
    return _startCapture(CaptureState::Streaming);
}

void AudioCapture::stopStreaming()
{
    if (_state == CaptureState::Streaming) {
        stopCapture();
    }
}

bool AudioCapture::startRecording()
{
    const bool started = _startCapture(CaptureState::Recording);
    if (started) {
        _safetyLimitTimer->start(kHardRecordingLimitMs);
    }
    return started;
}

QByteArray AudioCapture::stopRecording()
{
    if (_state != CaptureState::Recording) {
        return QByteArray();
    }

    _safetyLimitTimer->stop();
    const QByteArray recordedData = _recordedBuffer;
    stopCapture();
    return recordedData;
}

void AudioCapture::_onSafetyLimitReached()
{
    if (_state != CaptureState::Recording) {
        return;
    }

    qCWarning(AudioCaptureLog) << "Recording safety limit reached (60s). Force stopping capture.";

    const QByteArray recordedData = _recordedBuffer;
    stopCapture();

    emit recordingSafetyLimitReached(recordedData, _actualFormat);
}

void AudioCapture::stopCapture()
{
    if (_state == CaptureState::Stopped) {
        return;
    }

    _safetyLimitTimer->stop();
    _cleanupAudioSource();

    const CaptureState previousState = _state;
    _state = CaptureState::Stopped;

    if (_noiseFilter) {
        _noiseFilter->reset();
    }

    if (previousState == CaptureState::Recording) {
        emit recordingFinished(_recordedBuffer, _actualFormat);
    }

    emit captureStateChanged(_state);
}

bool AudioCapture::_startCapture(CaptureState state)
{
    if (_state != CaptureState::Stopped) {
        stopCapture();
    }

    if (!_deviceManager || !_deviceManager->isMicrophoneActive()) {
        qCWarning(AudioCaptureLog) << "Cannot start capture: No valid active microphone.";
        
        emit captureError(QStringLiteral("No active microphone available"));
        return false;
    }

    const QAudioDevice device = _deviceManager->selectedAudioDevice();
    if (device.isNull()) {
        qCWarning(AudioCaptureLog) << "Cannot start capture: Selected audio device is null.";

        emit captureError(QStringLiteral("Selected microphone is unavailable"));
        return false;
    }

    _actualFormat = _determineFormat(device);
    if (!_actualFormat.isValid()) {
        qCWarning(AudioCaptureLog) << "Cannot start capture: Failed to determine valid audio format.";

        emit captureError(QStringLiteral("Failed to negotiate audio format with hardware"));
        return false;
    }

    _cleanupAudioSource();

    _audioSource = new QAudioSource(device, _actualFormat, this);
    (void) connect(_audioSource, &QAudioSource::stateChanged, this, &AudioCapture::_onAudioSourceStateChanged);

    _audioIODevice = _audioSource->start();
    if (!_audioIODevice) {
        qCWarning(AudioCaptureLog) << "Failed to start QAudioSource IO device.";

        _cleanupAudioSource();
        emit captureError(QStringLiteral("Failed to open audio input stream"));
        return false;
    }

    (void) connect(_audioIODevice, &QIODevice::readyRead, this, &AudioCapture::_onReadyRead);

    _state = state;
    if (_state == CaptureState::Recording) {
        _recordedBuffer.clear();
    }

    if (_noiseFilter) {
        _noiseFilter->reset();
    }

    qCDebug(AudioCaptureLog) << "Audio capture started in mode:" << static_cast<int>(_state)
                             << "device:" << device.description() << "rate:" << _actualFormat.sampleRate()
                             << "channels:" << _actualFormat.channelCount()
                             << "noiseFilterEnabled:" << _noiseFilterEnabled
                             << "noiseFilterLevel:" << static_cast<int>(noiseFilterLevel());

    emit captureStateChanged(_state);
    return true;
}

void AudioCapture::_cleanupAudioSource()
{
    if (_audioIODevice) {
        _audioIODevice->disconnect(this);
        _audioIODevice = nullptr;
    }

    if (_audioSource) {
        _audioSource->disconnect(this);
        _audioSource->stop();
        delete _audioSource;
        _audioSource = nullptr;
    }
}

QAudioFormat AudioCapture::_determineFormat(const QAudioDevice& device)
{
    if (device.isFormatSupported(_targetFormat)) {
        return _targetFormat;
    }

    QAudioFormat preferred = device.preferredFormat();
    if (preferred.isValid()) {
        if (preferred.sampleFormat() != QAudioFormat::Int16 && preferred.sampleFormat() != QAudioFormat::Float) {
            preferred.setSampleFormat(QAudioFormat::Int16);
            if (!device.isFormatSupported(preferred)) {
                return device.preferredFormat();
            }
        }
        return preferred;
    }
    return _targetFormat;
}

void AudioCapture::_onReadyRead()
{
    if (!_audioIODevice) {
        return;
    }

    QByteArray chunk = _audioIODevice->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    if (_noiseFilterEnabled && _noiseFilter) {
        _noiseFilter->process(chunk, _actualFormat);
    }

    if (_state == CaptureState::Recording) {
        _recordedBuffer.append(chunk);
    } else if (_state == CaptureState::Streaming) {
        emit audioStreamFrameReady(chunk, _actualFormat);
    }
}

void AudioCapture::_onAudioSourceStateChanged(QAudio::State state)
{
    qCDebug(AudioCaptureLog) << "QAudioSource state changed:" << state;

    if (state == QAudio::StoppedState) {
        if (_audioSource && _audioSource->error() != QAudio::NoError) {
            qCWarning(AudioCaptureLog) << "QAudioSource error encountered:" << _audioSource->error();

            emit captureError(QStringLiteral("Audio hardware error occurred"));
            stopCapture();
        }
    }
}

void AudioCapture::_onDeviceAvailabilityChanged(bool active)
{
    qCDebug(AudioCaptureLog) << "Microphone active state changed:" << active;
    
    if (!active && _state != CaptureState::Stopped) {
        qCWarning(AudioCaptureLog) << "Active microphone disconnected while capturing. Stopping.";
        emit captureError(QStringLiteral("Microphone disconnected"));
        stopCapture();
    }
}

void AudioCapture::_onSelectedDeviceChanged()
{
    qCDebug(AudioCaptureLog) << "Selected microphone changed.";

    if (_state != CaptureState::Stopped) {
        const CaptureState currentState = _state;
        stopCapture();
        _startCapture(currentState);
    }
}
