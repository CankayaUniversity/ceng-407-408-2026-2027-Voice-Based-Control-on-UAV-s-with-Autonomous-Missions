#include "VoiceCommandManager.h"
#include "VoiceCommandWorker.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "Fact.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "SttEngineFactory.h"

QGC_LOGGING_CATEGORY(VoiceCommandManagerLog, "VoiceCommand.VoiceCommandManager")

Q_APPLICATION_STATIC(VoiceCommandManager, _voiceCommandManager);


VoiceCommandManager::VoiceCommandManager(AudioCapture* capture, AudioDeviceManager* deviceManager,
                                         SttModelManager* modelManager, QObject* parent)
    : QObject(parent),
      _audioCapture(capture ? capture : AudioCapture::instance()),
      _deviceManager(deviceManager ? deviceManager : AudioDeviceManager::instance()),
      _modelManager(modelManager ? modelManager : SttModelManager::instance()),
      _idleUnloadTimer(new QTimer(this))
{
    qRegisterMetaType<SttConfig>("SttConfig");
    qRegisterMetaType<SttResult>("SttResult");
    qRegisterMetaType<VoiceCommandManager::SessionState>("VoiceCommandManager::SessionState");

    _worker = new VoiceCommandWorker();
    _worker->moveToThread(&_workerThread);

    (void) connect(&_workerThread, &QThread::finished, _worker, &QObject::deleteLater);
    (void) connect(this, &VoiceCommandManager::_requestInitializeModel, _worker, &VoiceCommandWorker::initializeModel);
    (void) connect(this, &VoiceCommandManager::_requestUnloadModel, _worker, &VoiceCommandWorker::unloadModel);
    (void) connect(this, &VoiceCommandManager::_requestTranscribeAudio, _worker, &VoiceCommandWorker::transcribeAudio);
    (void) connect(_worker, &VoiceCommandWorker::modelInitialized, this, &VoiceCommandManager::_onModelInitialized);
    (void) connect(_worker, &VoiceCommandWorker::modelUnloaded, this, &VoiceCommandManager::_onModelUnloaded);
    (void) connect(_worker, &VoiceCommandWorker::transcriptionFinished, this,
                   &VoiceCommandManager::_onTranscriptionFinished);

    _workerThread.setObjectName(QStringLiteral("VoiceCommandWorkerThread"));
    _workerThread.start();

    _idleUnloadTimer->setSingleShot(true);
    (void) connect(_idleUnloadTimer, &QTimer::timeout, this, &VoiceCommandManager::_onIdleTimeout);

    if (_audioCapture) {
        (void) connect(_audioCapture, &AudioCapture::recordingFinished, this,
                       &VoiceCommandManager::_onRecordingFinished);
        (void) connect(_audioCapture, &AudioCapture::recordingSafetyLimitReached, this,
                       &VoiceCommandManager::_onRecordingSafetyLimitReached);
        (void) connect(_audioCapture, &AudioCapture::captureStateChanged, this,
                       &VoiceCommandManager::_onCaptureStateChanged);
    }

    if (_deviceManager) {
        (void) connect(_deviceManager, &AudioDeviceManager::isMicrophoneActiveChanged, this,
                       &VoiceCommandManager::_onMicrophoneActiveChanged);
    }

    if (_modelManager) {
        (void) connect(_modelManager, &SttModelManager::isModelAvailableChanged, this,
                       &VoiceCommandManager::_onSttModelAvailableChanged);
    }

    if (MultiVehicleManager::instance()) {
        (void) connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged, this, [this]() {
            emit vehicleConnectedChanged(isVehicleConnected());
            emit readyChanged(isReady());
        });
    }
}

VoiceCommandManager::~VoiceCommandManager()
{
    if (_idleUnloadTimer) {
        _idleUnloadTimer->stop();
    }

    if (_isEventFilterInstalled && qApp) {
        qApp->removeEventFilter(this);
        _isEventFilterInstalled = false;
    }

    _workerThread.quit();
    _workerThread.wait();
}

VoiceCommandManager* VoiceCommandManager::instance()
{
    return _voiceCommandManager();
}

void VoiceCommandManager::init(Fact* pushToTalkKeyFact, Fact* sttSelectedModelFact)
{
    if (_pushToTalkKeyFact) {
        disconnect(_pushToTalkKeyFact, &Fact::rawValueChanged, this, &VoiceCommandManager::_onFactValueChanged);
    }

    _pushToTalkKeyFact = pushToTalkKeyFact;

    if (_pushToTalkKeyFact) {
        const QString savedKey = _pushToTalkKeyFact->rawValue().toString().trimmed();
        if (!savedKey.isEmpty()) {
            _pushToTalkKey = savedKey;
        }
        (void) connect(_pushToTalkKeyFact, &Fact::rawValueChanged, this, &VoiceCommandManager::_onFactValueChanged);
    }

    if (_modelManager) {
        _modelManager->init(sttSelectedModelFact);
    }

    if (!_isEventFilterInstalled && qApp) {
        qApp->installEventFilter(this);
        _isEventFilterInstalled = true;
        qCDebug(VoiceCommandManagerLog) << "Global PTT shortcut event filter installed. Key:" << _pushToTalkKey;
    }
}

QString VoiceCommandManager::sessionStateString() const
{
    switch (_sessionState) {
    case SessionState::Idle:
        return QStringLiteral("Idle");
    case SessionState::Listening:
        return QStringLiteral("Listening");
    case SessionState::Processing:
        return QStringLiteral("Processing");
    case SessionState::Confirming:
        return QStringLiteral("Confirming");
    case SessionState::Executing:
        return QStringLiteral("Executing");
    case SessionState::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Idle");
}

void VoiceCommandManager::_setSessionState(SessionState newState)
{
    if (_sessionState != newState) {
        _sessionState = newState;
        emit sessionStateChanged(_sessionState);
        qCDebug(VoiceCommandManagerLog) << "Session state changed to:" << sessionStateString();
    }
}

void VoiceCommandManager::resetSession()
{
    if (isRecording()) {
        stopRecording();
    }
    _setSessionState(SessionState::Idle);
}

bool VoiceCommandManager::isRecording() const
{
    return _audioCapture ? _audioCapture->isRecording() : false;
}

QString VoiceCommandManager::pushToTalkKey() const
{
    return _pushToTalkKey;
}

void VoiceCommandManager::setPushToTalkKey(const QString& key)
{
    const QString trimmed = key.trimmed();
    if (_pushToTalkKey == trimmed || trimmed.isEmpty()) {
        return;
    }

    _pushToTalkKey = trimmed;

    if (_pushToTalkKeyFact && _pushToTalkKeyFact->rawValue().toString() != trimmed) {
        _pushToTalkKeyFact->setRawValue(trimmed);
    }

    emit pushToTalkKeyChanged(_pushToTalkKey);
    qCDebug(VoiceCommandManagerLog) << "Push-to-Talk shortcut updated to:" << _pushToTalkKey;
}

bool VoiceCommandManager::isMicrophoneActive() const
{
    return _deviceManager ? _deviceManager->isMicrophoneActive() : false;
}

bool VoiceCommandManager::isVehicleConnected() const
{
    if (!_vehicleCheckEnabled) {
        return true;
    }
    MultiVehicleManager* mvm = MultiVehicleManager::instance();
    return mvm ? (mvm->activeVehicle() != nullptr) : false;
}

void VoiceCommandManager::setVehicleCheckEnabled(bool enabled)
{
    if (_vehicleCheckEnabled != enabled) {
        _vehicleCheckEnabled = enabled;
        emit vehicleConnectedChanged(isVehicleConnected());
        emit readyChanged(isReady());
    }
}

bool VoiceCommandManager::isSttModelAvailable() const
{
    if (!_sttModelCheckEnabled) {
        return true;
    }
    return _modelManager ? _modelManager->isModelAvailable() : false;
}

bool VoiceCommandManager::isReady() const
{
    return isMicrophoneActive() && isVehicleConnected() && isSttModelAvailable();
}

void VoiceCommandManager::setSttModelCheckEnabled(bool enabled)
{
    if (_sttModelCheckEnabled != enabled) {
        _sttModelCheckEnabled = enabled;
        emit sttModelAvailableChanged(isSttModelAvailable());
        emit readyChanged(isReady());
    }
}

bool VoiceCommandManager::isSttModelLoaded() const
{
    return _isModelLoaded;
}

void VoiceCommandManager::setSttEngine(SpeechToTextInterface* engine, bool takeOwnership)
{
    if (_worker) {
        if (_workerThread.isRunning()) {
            QMetaObject::invokeMethod(
                _worker,
                [worker = _worker, engine, takeOwnership]() {
                    worker->setEngine(engine, takeOwnership);
                },
                Qt::BlockingQueuedConnection);
        } else {
            _worker->setEngine(engine, takeOwnership);
        }
    }
    _isModelLoaded = (engine && engine->isLoaded());
    emit modelLoadedChanged(_isModelLoaded);
}

SpeechToTextInterface* VoiceCommandManager::sttEngine() const
{
    return _worker ? _worker->engine() : nullptr;
}

void VoiceCommandManager::setIdleUnloadTimeout(int timeoutMs)
{
    _idleUnloadTimeoutMs = timeoutMs;
}

void VoiceCommandManager::setKeepModelInMemory(bool keepInMemory)
{
    _keepModelInMemory = keepInMemory;
    if (_keepModelInMemory && _idleUnloadTimer) {
        _idleUnloadTimer->stop();
    }
}

bool VoiceCommandManager::startRecording()
{
    if (!_audioCapture) {
        qCWarning(VoiceCommandManagerLog) << "Cannot start recording: AudioCapture unavailable";
        return false;
    }

    if (!isVehicleConnected()) {
        qCDebug(VoiceCommandManagerLog) << "Cannot start recording: No active vehicle connected";
        return false;
    }

    if (!isSttModelAvailable()) {
        const QString msg = tr("Model for voice recognition is not found");
        qCWarning(VoiceCommandManagerLog) << msg;
        emit voiceCommandWarning(msg);
        if (qgcApp() && !qgcApp()->runningUnitTests()) {
            qgcApp()->showAppMessage(msg);
        }
        return false;
    }

    if (!isMicrophoneActive()) {
        qCDebug(VoiceCommandManagerLog) << "Cannot start recording: Microphone is not active";
        return false;
    }

    if (isRecording()) {
        return true;
    }

    qCDebug(VoiceCommandManagerLog) << "Starting Push-to-Talk voice recording...";
    const bool started = _audioCapture->startRecording();
    if (started) {
        _setSessionState(SessionState::Listening);
        
        if (!_isModelLoaded && _modelManager && _modelManager->isModelAvailable()) {
            const SttConfig config = _modelManager->currentResolvedConfig(0, false);
            qCDebug(VoiceCommandManagerLog) << "Pre-warming STT model in worker thread on PTT press:" << config.modelPath;
            emit _requestInitializeModel(config);
        }
    }
    return started;
}

void VoiceCommandManager::stopRecording()
{
    if (!_audioCapture || !isRecording()) {
        return;
    }

    qCDebug(VoiceCommandManagerLog) << "Stopping Push-to-Talk voice recording...";
    (void) _audioCapture->stopRecording();
}

bool VoiceCommandManager::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);

    if (!event) {
        return false;
    }

    const QEvent::Type type = event->type();
    if (type != QEvent::KeyPress && type != QEvent::KeyRelease) {
        return false;
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->isAutoRepeat()) {
        return false;
    }

    if (type == QEvent::KeyPress) {
        if (_isTextInputFocused()) {
            return false;
        }

        if (_matchesPttKey(keyEvent->key(), keyEvent->text())) {
            if (!isVehicleConnected()) {
                qCDebug(VoiceCommandManagerLog) << "PTT key ignored: No active vehicle connected";
                return false;
            }
            _isKeyPressed = true;
            (void) startRecording();
            return true;
        }
    } else if (type == QEvent::KeyRelease) {
        if (_isKeyPressed && _matchesPttKey(keyEvent->key(), keyEvent->text())) {
            _isKeyPressed = false;
            stopRecording();
            return true;
        }
    }

    return false;
}

void VoiceCommandManager::_onFactValueChanged()
{
    if (!_pushToTalkKeyFact) {
        return;
    }

    const QString newKey = _pushToTalkKeyFact->rawValue().toString().trimmed();
    if (!newKey.isEmpty() && newKey != _pushToTalkKey) {
        _pushToTalkKey = newKey;
        emit pushToTalkKeyChanged(_pushToTalkKey);
        qCDebug(VoiceCommandManagerLog) << "Push-to-Talk key updated from Fact:" << _pushToTalkKey;
    }
}

void VoiceCommandManager::_onRecordingFinished(const QByteArray& recordedPcmData, const QAudioFormat& format)
{
    qCDebug(VoiceCommandManagerLog) << "Recording finished, buffer bytes:" << recordedPcmData.size();

    constexpr int kMinPcmBytes = 16000;
    const qint64 durationUs = format.isValid() ? format.durationForBytes(recordedPcmData.size())
                                               : (recordedPcmData.size() * 1000000LL / 32000LL);

    if (durationUs < 500000 || recordedPcmData.size() < kMinPcmBytes) {
        qCDebug(VoiceCommandManagerLog) << "Utterance rejected: duration" << durationUs
                                       << "us (< 500ms) or size" << recordedPcmData.size() << "bytes (< 16000 bytes)";
        _setSessionState(SessionState::Idle);
        const QString msg = tr("Voice Command is too short to recognize");
        emit voiceCommandWarning(msg);
        if (qgcApp() && !qgcApp()->runningUnitTests()) {
            qgcApp()->showAppMessage(msg);
        }
        return;
    }

    emit voiceCommandRecorded(recordedPcmData, format);

    _setSessionState(SessionState::Processing);

    SttConfig config;
    if (_modelManager) {
        config = _modelManager->currentResolvedConfig(0, false);
    }
    emit _requestTranscribeAudio(recordedPcmData, config);
}

void VoiceCommandManager::_onRecordingSafetyLimitReached(const QByteArray& recordedPcmData, const QAudioFormat& format)
{
    qCWarning(VoiceCommandManagerLog) << "Voice command hit 60s memory safety ceiling!";
    emit recordingSafetyLimitReached(recordedPcmData, format);
    _onRecordingFinished(recordedPcmData, format);
}

void VoiceCommandManager::_onMicrophoneActiveChanged(bool active)
{
    emit microphoneActiveChanged(active);
    emit readyChanged(isReady());
}

void VoiceCommandManager::_onSttModelAvailableChanged(bool available)
{
    Q_UNUSED(available);
    emit sttModelAvailableChanged(isSttModelAvailable());
    emit readyChanged(isReady());
}

void VoiceCommandManager::_onCaptureStateChanged()
{
    emit isRecordingChanged(isRecording());
}

void VoiceCommandManager::_onModelInitialized(bool success)
{
    if (_isModelLoaded != success) {
        _isModelLoaded = success;
        emit modelLoadedChanged(_isModelLoaded);
    }
    qCDebug(VoiceCommandManagerLog) << "STT model initialized in worker thread, loaded:" << _isModelLoaded;
}

void VoiceCommandManager::_onModelUnloaded()
{
    if (_isModelLoaded) {
        _isModelLoaded = false;
        emit modelLoadedChanged(false);
    }
    qCDebug(VoiceCommandManagerLog) << "STT model unloaded in worker thread";
}

void VoiceCommandManager::_onTranscriptionFinished(const SttResult& result)
{
    qCDebug(VoiceCommandManagerLog) << "STT inference finished. Success:" << result.success
                                   << "Time:" << result.inferenceTimeMs << "ms";

    if (_idleUnloadTimer && _idleUnloadTimeoutMs > 0) {
        _idleUnloadTimer->start(_idleUnloadTimeoutMs);
    }

    if (!result.success || result.text.trimmed().isEmpty()) {
        qCWarning(VoiceCommandManagerLog) << "STT transcription failed:" << result.errorMessage;
        _setSessionState(SessionState::Error);
        const QString msg = tr("Voice Command is Failed");
        emit voiceCommandWarning(msg);
        if (qgcApp() && !qgcApp()->runningUnitTests()) {
            qgcApp()->showAppMessage(msg);
        }
        _setSessionState(SessionState::Idle);
        return;
    }

    _isModelLoaded = true;
    const QString transcript = result.text.trimmed();
    qCDebug(VoiceCommandManagerLog) << "Transcription output:" << transcript;
    emit voiceCommandTranscribed(transcript, result.confidence);

    _setSessionState(SessionState::Confirming);
}

void VoiceCommandManager::_onIdleTimeout()
{
    if (_sessionState != SessionState::Listening && _sessionState != SessionState::Processing && !_keepModelInMemory) {
        qCDebug(VoiceCommandManagerLog) << "Idle unload timeout reached; releasing STT model from memory";
        emit _requestUnloadModel();
    }
}

bool VoiceCommandManager::_matchesPttKey(int key, const QString& text) const
{
    if (_pushToTalkKey.isEmpty()) {
        return false;
    }

    if (_pushToTalkKey.length() == 1) {
        if (!text.isEmpty() && text.compare(_pushToTalkKey, Qt::CaseInsensitive) == 0) {
            return true;
        }
        const QChar targetChar = _pushToTalkKey.at(0).toUpper();
        if (key == targetChar.unicode()) {
            return true;
        }
    } else {
        const QKeySequence seq(_pushToTalkKey);
        if (!seq.isEmpty() && key == seq[0].toCombined()) {
            return true;
        }
    }

    return false;
}

bool VoiceCommandManager::_isTextInputFocused()
{
    QObject* focusObj = QGuiApplication::focusObject();
    if (!focusObj) {
        return false;
    }

    const QString className = QString::fromLatin1(focusObj->metaObject()->className());
    return (className.contains(QStringLiteral("TextInput"), Qt::CaseInsensitive) ||
            className.contains(QStringLiteral("TextEdit"), Qt::CaseInsensitive) ||
            className.contains(QStringLiteral("TextField"), Qt::CaseInsensitive) ||
            className.contains(QStringLiteral("FactTextField"), Qt::CaseInsensitive) ||
            className.contains(QStringLiteral("LineEdit"), Qt::CaseInsensitive));
}
