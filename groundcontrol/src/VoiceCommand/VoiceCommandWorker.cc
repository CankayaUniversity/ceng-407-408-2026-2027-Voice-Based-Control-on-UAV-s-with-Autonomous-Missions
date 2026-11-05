#include "VoiceCommandWorker.h"

#include "SttEngineFactory.h"

VoiceCommandWorker::VoiceCommandWorker(QObject* parent) : QObject(parent) {}

VoiceCommandWorker::~VoiceCommandWorker()
{
    if (_ownsEngine && _engine) {
        delete _engine;
        _engine = nullptr;
    }
}

void VoiceCommandWorker::setEngine(SpeechToTextInterface* engine, bool takeOwnership)
{
    if (_ownsEngine && _engine && _engine != engine) {
        delete _engine;
    }
    _engine = engine;
    _ownsEngine = takeOwnership;
}

void VoiceCommandWorker::initializeModel(const SttConfig& config)
{
    if (!_engine) {
        setEngine(SttEngineFactory::createForModelPath(config.modelPath).release(), true);
    }
    if (!_engine) {
        emit modelInitialized(false);
        return;
    }
    const bool ok = _engine->initialize(config);
    _currentConfig = config;
    emit modelInitialized(ok);
}

void VoiceCommandWorker::unloadModel()
{
    if (_engine) {
        _engine->unload();
    }
    emit modelUnloaded();
}

void VoiceCommandWorker::transcribeAudio(const QByteArray& pcmData, const SttConfig& config)
{
    if (!_engine) {
        setEngine(SttEngineFactory::createForModelPath(config.modelPath).release(), true);
    }
    if (!_engine) {
        SttResult err;
        err.success = false;
        err.errorMessage = QStringLiteral("No STT engine available for model: ") + config.modelPath;
        emit transcriptionFinished(err);
        return;
    }
    if (!_engine->isLoaded()) {
        if (!_engine->initialize(config)) {
            SttResult err;
            err.success = false;
            err.errorMessage = QStringLiteral("Failed to initialize STT model: ") + config.modelPath;
            emit transcriptionFinished(err);
            return;
        }
    }
    const SttResult res = _engine->transcribe(pcmData);
    emit transcriptionFinished(res);
}
