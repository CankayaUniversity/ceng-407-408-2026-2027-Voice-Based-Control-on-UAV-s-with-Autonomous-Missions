#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "SpeechToTextInterface.h"

/// \brief Background worker executing STT model loading, inference, and memory release
/// on a dedicated QThread so QGC GUI thread remains 100% responsive.
class VoiceCommandWorker : public QObject
{
    Q_OBJECT

public:
    explicit VoiceCommandWorker(QObject* parent = nullptr);
    ~VoiceCommandWorker() override;

    void setEngine(SpeechToTextInterface* engine, bool takeOwnership);

    SpeechToTextInterface* engine() const { return _engine; }

public slots:
    void initializeModel(const SttConfig& config);
    void unloadModel();
    void transcribeAudio(const QByteArray& pcmData, const SttConfig& config);

signals:
    void modelInitialized(bool success);
    void modelUnloaded();
    void transcriptionFinished(const SttResult& result);

private:
    SpeechToTextInterface* _engine = nullptr;
    bool _ownsEngine = false;
    SttConfig _currentConfig;
};
