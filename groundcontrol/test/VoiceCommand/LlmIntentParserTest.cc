#include "LlmIntentParserTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

#include "LlmIntentParser.h"

namespace {

QByteArray response(const QByteArray& commands)
{
    const QJsonObject body{
        {QStringLiteral("choices"),
         QJsonArray{QJsonObject{
             {QStringLiteral("message"), QJsonObject{{QStringLiteral("content"), QString::fromUtf8(commands)}}}}}}};
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

class MockLlmServer
{
public:
    MockLlmServer()
    {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this]() {
            QTcpSocket* socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, bytes = QByteArray()]() mutable {
                bytes += socket->readAll();
                const int headerEnd = bytes.indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }
                const QByteArray headers = bytes.left(headerEnd);
                requestHeaders = headers;
                const int start = headers.toLower().indexOf("content-length: ");
                if (start < 0) {
                    return;
                }
                const int end = headers.indexOf("\r\n", start);
                const int length = headers.mid(start + 16, end < 0 ? -1 : end - start - 16).toInt();
                if (bytes.size() < headerEnd + 4 + length) {
                    return;
                }
                request = QJsonDocument::fromJson(bytes.mid(headerEnd + 4, length)).object();
                const QByteArray http = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                                        QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                socket->write(http);
                socket->disconnectFromHost();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
    }

    bool listen() { return server.listen(QHostAddress::LocalHost); }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort()));
    }

    QTcpServer server;
    QByteArray body;
    QByteArray requestHeaders;
    QJsonObject request;
};

}  // namespace

void LlmIntentParserTest::_testRequestAndSequence()
{
    MockLlmServer server;
    QVERIFY(server.listen());
    server.body = response(
        R"({"intents":[{"action":"Arm","parameters":{}},{"action":"Takeoff","parameters":{"altitude":10}},{"action":"Move","parameters":{"direction":"left","distance":5,"speed":2}}]})");
    LlmIntentParser parser(server.url(), QStringLiteral("test-model"), QStringLiteral("secret"));
    VehicleStateSnapshot state;
    state.vehicleId = 42;
    state.isArmed = false;
    IntentParseResult result;
    bool done = false;
    parser.parse(QStringLiteral("launch to 10 meters then move left five meters"), state,
                 [&](IntentParseResult parsed) {
                     result = std::move(parsed);
                     done = true;
                 });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY(result.success);
    QCOMPARE(
        result.llmResponse,
        QString::fromUtf8(
            R"({"intents":[{"action":"Arm","parameters":{}},{"action":"Takeoff","parameters":{"altitude":10}},{"action":"Move","parameters":{"direction":"left","distance":5,"speed":2}}]})"));
    QCOMPARE(result.intents.size(), 3);
    QCOMPARE(result.intents.at(0).action, VoiceCommandAction::Arm);
    QCOMPARE(result.intents.at(1).action, VoiceCommandAction::Takeoff);
    QCOMPARE(result.intents.at(1).parameters.value(QStringLiteral("altitude")).toDouble(), 10.0);
    QCOMPARE(result.intents.at(2).action, VoiceCommandAction::Move);
    QCOMPARE(result.intents.at(2).parameters.value(QStringLiteral("speed")).toDouble(), 2.0);
    QCOMPARE(result.intents.at(2).targetVehicleId, 42);
    QCOMPARE(server.request.value(QStringLiteral("model")).toString(), QStringLiteral("test-model"));
    QVERIFY(server.requestHeaders.contains("Authorization: Bearer secret"));
    const QJsonArray messages = server.request.value(QStringLiteral("messages")).toArray();
    QCOMPARE(messages.size(), 2);
    QVERIFY(messages.at(1)
                .toObject()
                .value(QStringLiteral("content"))
                .toString()
                .contains(QStringLiteral("launch to 10 meters")));
    QVERIFY(messages.at(1)
                .toObject()
                .value(QStringLiteral("content"))
                .toString()
                .contains(QStringLiteral("\"isArmed\":false")));
}

void LlmIntentParserTest::_testSupportedActions()
{
    MockLlmServer server;
    QVERIFY(server.listen());
    server.body = response(
        R"({"intents":[{"action":"Disarm","parameters":{}},{"action":"Land","parameters":{}},{"action":"Hold","parameters":{}},{"action":"ReturnToLaunch","parameters":{}},{"action":"Takeoff","parameters":{}},{"action":"ChangeAltitude","parameters":{"altitude":12}},{"action":"ChangeHeading","parameters":{"heading":0}},{"action":"Move","parameters":{"direction":"north","distance":3}}]})");
    LlmIntentParser parser(server.url(), QStringLiteral("test-model"));
    bool done = false;
    IntentParseResult result;
    parser.parse(QStringLiteral("test actions"), {}, [&](IntentParseResult parsed) {
        result = std::move(parsed);
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY(result.success);
    QCOMPARE(result.intents.size(), 8);
    QCOMPARE(result.intents.at(0).action, VoiceCommandAction::Disarm);
    QCOMPARE(result.intents.at(1).action, VoiceCommandAction::Land);
    QCOMPARE(result.intents.at(2).action, VoiceCommandAction::Hold);
    QCOMPARE(result.intents.at(3).action, VoiceCommandAction::ReturnToLaunch);
    QCOMPARE(result.intents.at(4).action, VoiceCommandAction::Takeoff);
    QCOMPARE(result.intents.at(5).parameters.value(QStringLiteral("altitude")).toDouble(), 12.0);
    QCOMPARE(result.intents.at(6).parameters.value(QStringLiteral("heading")).toDouble(), 0.0);
    QCOMPARE(result.intents.at(7).parameters.value(QStringLiteral("direction")).toString(), QStringLiteral("north"));
    for (const MissionIntent& intent : result.intents) {
        QVERIFY(!intent.displaySummary.isEmpty());
    }
}

void LlmIntentParserTest::_testRejectsInvalidCommands()
{
    MockLlmServer server;
    QVERIFY(server.listen());
    LlmIntentParser parser(server.url(), QStringLiteral("test-model"));
    const QList<QByteArray> invalid{
        R"({"intents":[]})",
        R"({"intents":[{"action":"DeleteMission","parameters":{}}]})",
        R"({"intents":[{"action":"Arm","parameters":{}},{"action":"Move","parameters":{"direction":"left"}}]})",
        R"({"intents":[{"action":"Move","parameters":{"direction":"left","distance":-1}}]})",
        R"({"intents":[{"action":"ChangeHeading","parameters":{"heading":400}}]})",
        R"({"intents":[{"action":"Land","parameters":{},"targetVehicleId":99}]})",
        R"({"intents":[{"action":"Arm","parameters":{"altitude":10}}]})",
        R"(not json)"};
    for (const QByteArray& command : invalid) {
        server.body = response(command);
        bool done = false;
        IntentParseResult result;
        parser.parse(QStringLiteral("test"), {}, [&](IntentParseResult parsed) {
            result = std::move(parsed);
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
        QVERIFY2(!result.success, command.constData());
        QVERIFY(result.intents.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.llmResponse, QString::fromUtf8(command));
    }
}

void LlmIntentParserTest::_testNetworkFailure()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort()));
    server.close();
    LlmIntentParser parser(url, QStringLiteral("test-model"));
    bool done = false;
    IntentParseResult result;
    parser.parse(QStringLiteral("land"), {}, [&](IntentParseResult parsed) {
        result = std::move(parsed);
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY(!result.success);
    QVERIFY(result.intents.isEmpty());
}

UT_REGISTER_TEST(LlmIntentParserTest, TestLabel::Unit, TestLabel::Utilities)
