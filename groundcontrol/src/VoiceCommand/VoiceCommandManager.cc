#include "VoiceCommandManager.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>

#include "AudioCapture.h"
#include "AudioDeviceManager.h"
#include "Fact.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(VoiceCommandManagerLog, "VoiceCommand.VoiceCommandManager")

Q_APPLICATION_STATIC(VoiceCommandManager, _voiceCommandManager);

VoiceCommandManager::VoiceCommandManager(AudioCapture* capture, AudioDeviceManager* deviceManager, QObject* parent)
    : QObject(parent),
      _audioCapture(capture ? capture : AudioCapture::instance()),
      _deviceManager(deviceManager ? deviceManager : AudioDeviceManager::instance())
{
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

    if (MultiVehicleManager::instance()) {
        (void) connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged, this,
                       [this]() { emit vehicleConnectedChanged(isVehicleConnected()); });
    }
}

VoiceCommandManager::~VoiceCommandManager()
{
    if (_isEventFilterInstalled && qApp) {
        qApp->removeEventFilter(this);
        _isEventFilterInstalled = false;
    }
}

VoiceCommandManager* VoiceCommandManager::instance()
{
    return _voiceCommandManager();
}

void VoiceCommandManager::init(Fact* pushToTalkKeyFact)
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

    if (!_isEventFilterInstalled && qApp) {
        qApp->installEventFilter(this);
        _isEventFilterInstalled = true;
        qCDebug(VoiceCommandManagerLog) << "Global PTT shortcut event filter installed. Key:" << _pushToTalkKey;
    }
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
    }
}

bool VoiceCommandManager::startRecording()
{
    if (!_audioCapture) {
        qCWarning(VoiceCommandManagerLog) << "Cannot start recording: AudioCapture unavailable";
        return false;
    }

    if (!isMicrophoneActive()) {
        qCDebug(VoiceCommandManagerLog) << "Cannot start recording: Microphone is not active";
        return false;
    }

    if (!isVehicleConnected()) {
        qCDebug(VoiceCommandManagerLog) << "Cannot start recording: No active vehicle connected";
        return false;
    }

    if (isRecording()) {
        return true;
    }

    qCDebug(VoiceCommandManagerLog) << "Starting Push-to-Talk voice recording...";
    return _audioCapture->startRecording();
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
    emit voiceCommandRecorded(recordedPcmData, format);
}

void VoiceCommandManager::_onRecordingSafetyLimitReached(const QByteArray& recordedPcmData, const QAudioFormat& format)
{
    qCWarning(VoiceCommandManagerLog) << "Voice command hit 60s memory safety ceiling!";
    emit recordingSafetyLimitReached(recordedPcmData, format);
}

void VoiceCommandManager::_onMicrophoneActiveChanged(bool active)
{
    emit microphoneActiveChanged(active);
}

void VoiceCommandManager::_onCaptureStateChanged()
{
    emit isRecordingChanged(isRecording());
}

bool VoiceCommandManager::_matchesPttKey(int key, const QString& text) const
{
    if (_pushToTalkKey.isEmpty()) {
        return false;
    }

    // Match single-character key (case-insensitive)
    if (_pushToTalkKey.length() == 1) {
        if (!text.isEmpty() && text.compare(_pushToTalkKey, Qt::CaseInsensitive) == 0) {
            return true;
        }
        const QChar targetChar = _pushToTalkKey.at(0).toUpper();
        if (key == targetChar.unicode()) {
            return true;
        }
    } else {
        // Multi-character sequence e.g. "Space", "F1"
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
            className.contains(QStringLiteral("FactTextField"), Qt::CaseInsensitive));
}
