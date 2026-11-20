#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>

#include "IntentParserInterface.h"

/// Parses transcripts through an OpenAI-compatible chat completions endpoint.
/// The model's output is untrusted: only validated actions and parameters are returned.
class LlmIntentParser : public QObject, public IntentParserInterface
{
    Q_OBJECT

public:
    explicit LlmIntentParser(const QUrl& endpoint = {}, const QString& model = {}, const QString& apiKey = {},
                             QObject* parent = nullptr);
    void parse(const QString& transcript, const VehicleStateSnapshot& vehicleState, Completion completion) override;

private:
    QNetworkAccessManager _network;
    QUrl _endpoint;
    QString _model;
    QString _apiKey;
};
