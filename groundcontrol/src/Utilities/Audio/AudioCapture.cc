#include "AudioCapture.h"

#include <algorithm>

#include "AudioDeviceManager.h"
#include "Fact.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AudioCaptureLog, "Utilities.AudioCapture")

Q_APPLICATION_STATIC(AudioCapture, _audioCapture);

AudioCapture::AudioCapture(AudioDeviceManager* deviceManager, QObject* parent)
    : QObject(parent),
      _deviceManager(deviceManager ? deviceManager : AudioDeviceManager::instance()),
      _safetyLimitTimer(new QTimer(this))
{
    _targetFormat.setSampleRate(16000);
    _targetFormat.setChannelCount(1);
    _targetFormat.setSampleFormat(QAudioFormat::Int16);

    _safetyLimitTimer->setSingleShot(true);
    (void) connect(_safetyLimitTimer, &QTimer::timeout, this, &AudioCapture::_onSafetyLimitReached);

    if (_deviceManager) {
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

void AudioCapture::init(Fact* noiseFilterStrengthFact)
{
    if (_noiseFilterStrengthFact == noiseFilterStrengthFact) {
        return;
    }

    if (_noiseFilterStrengthFact) {
        (void) disconnect(_noiseFilterStrengthFact, &Fact::rawValueChanged, this,
                          &AudioCapture::_onNoiseFactValueChanged);
    }

    _noiseFilterStrengthFact = noiseFilterStrengthFact;

    if (_noiseFilterStrengthFact) {
        (void) connect(_noiseFilterStrengthFact, &Fact::rawValueChanged, this, &AudioCapture::_onNoiseFactValueChanged);
        _onNoiseFactValueChanged();
    }
}

void AudioCapture::_onNoiseFactValueChanged()
{
    if (!_noiseFilterStrengthFact) {
        return;
    }

    const int strength = _noiseFilterStrengthFact->rawValue().toInt();
    if (_noiseFilter.strength() != strength) {
        setNoiseFilterStrength(strength);
    }
}

void AudioCapture::setNoiseFilterStrength(int strengthPercent)
{
    const int clamped = std::clamp(strengthPercent, 0, 100);
    if (_noiseFilter.strength() == clamped) {
        return;
    }

    _noiseFilter.setStrength(clamped);

    if (_noiseFilterStrengthFact && _noiseFilterStrengthFact->rawValue().toInt() != clamped) {
        _noiseFilterStrengthFact->setRawValue(clamped);
    }

    emit noiseFilterStrengthChanged(clamped);
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

    qCWarning(AudioCaptureLog) << "Safety recording limit reached (60s). Stopping capture to prevent memory overflow.";
    const QByteArray buffer = _recordedBuffer;
    const QAudioFormat format = _actualFormat;

    stopCapture();
    emit recordingSafetyLimitReached(buffer, format);
}

void AudioCapture::stopCapture()
{
    if (_state == CaptureState::Stopped) {
        return;
    }

    _safetyLimitTimer->stop();

    const CaptureState previousState = _state;
    const QByteArray finishedBuffer = _recordedBuffer;
    _state = CaptureState::Stopped;

    _cleanupAudioSource();
    _noiseFilter.reset();

    if (previousState == CaptureState::Recording) {
        emit recordingFinished(finishedBuffer, _actualFormat);
    }

    emit captureStateChanged(_state);
}

bool AudioCapture::_startCapture(CaptureState state)
{
    if (_state == state) {
        return true;
    }

    if (!_deviceManager || !_deviceManager->isMicrophoneActive()) {
        qCWarning(AudioCaptureLog) << "Cannot start capture: no active microphone selected.";

        emit captureError(QStringLiteral("No active microphone available"));
        return false;
    }

    const QAudioDevice device = _deviceManager->selectedAudioDevice();
    if (device.isNull()) {
        qCWarning(AudioCaptureLog) << "Cannot start capture: selected QAudioDevice is null.";

        emit captureError(QStringLiteral("Selected audio device is null"));
        return false;
    }

    const QAudioFormat previousFormat = _actualFormat;
    _actualFormat = _determineFormat(device);

    if (_actualFormat != previousFormat) {
        emit audioFormatChanged(_actualFormat);
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

    _noiseFilter.reset();
    qCDebug(AudioCaptureLog) << "Audio capture started in mode:" << static_cast<int>(_state)
                             << "device:" << device.description() << "rate:" << _actualFormat.sampleRate()
                             << "channels:" << _actualFormat.channelCount()
                             << "noiseFilterStrength:" << _noiseFilter.strength();

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

    _noiseFilter.process(chunk, _actualFormat);

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
    if (!active && _state != CaptureState::Stopped) {
        qCWarning(AudioCaptureLog) << "Selected microphone became unavailable during capture.";

        stopCapture();
        emit captureError(QStringLiteral("Microphone disconnected"));
    }
}

void AudioCapture::_onSelectedDeviceChanged()
{
    if (_state != CaptureState::Stopped) {
        qCDebug(AudioCaptureLog) << "Selected microphone changed during capture. Restarting capture...";

        const CaptureState currentState = _state;
        stopCapture();
        _startCapture(currentState);
    }
}
