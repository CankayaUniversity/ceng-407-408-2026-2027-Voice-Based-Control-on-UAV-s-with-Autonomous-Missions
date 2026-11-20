#include "DeterministicMatcherTest.h"

#include <QtCore/QElapsedTimer>

#include "DeterministicMatcher.h"

void DeterministicMatcherTest::_testCoreActions()
{
    DeterministicMatcher matcher;
    const QList<QPair<QString, VoiceCommandAction>> commands{
        {QStringLiteral("arm"), VoiceCommandAction::Arm},
        {QStringLiteral("disarm"), VoiceCommandAction::Disarm},
        {QStringLiteral("take off"), VoiceCommandAction::Takeoff},
        {QStringLiteral("land"), VoiceCommandAction::Land},
        {QStringLiteral("hold position"), VoiceCommandAction::Hold},
        {QStringLiteral("return to launch"), VoiceCommandAction::ReturnToLaunch},
        {QStringLiteral("change altitude to 12 meters"), VoiceCommandAction::ChangeAltitude},
        {QStringLiteral("change heading to 90 degrees"), VoiceCommandAction::ChangeHeading},
        {QStringLiteral("move forward 10 meters"), VoiceCommandAction::Move},
    };
    for (const auto& [text, action] : commands) {
        const IntentParseResult result = matcher.parse(text);
        QVERIFY2(result.success, qPrintable(text));
        QCOMPARE(result.intents.size(), 1);
        QCOMPARE(result.intents.first().action, action);
        QVERIFY(!result.intents.first().displaySummary.isEmpty());
    }
}

void DeterministicMatcherTest::_testSlotsAndSequence()
{
    DeterministicMatcher matcher;
    VehicleStateSnapshot vehicle;
    vehicle.vehicleId = 42;

    (void) matcher.parse(QStringLiteral("arm"));
    QElapsedTimer timer;
    timer.start();
    IntentParseResult sequence;
    for (int i = 0; i < 20; ++i) {
        sequence = matcher.parse(QStringLiteral("Arm and take off to 10 meters."), vehicle);
    }
    QVERIFY(timer.elapsed() < 200);  // <10ms per command after initialization
    QVERIFY(sequence.success);
    QCOMPARE(sequence.rawTranscript, QStringLiteral("Arm and take off to 10 meters."));
    QCOMPARE(sequence.intents.size(), 2);
    QCOMPARE(sequence.intents.at(0).action, VoiceCommandAction::Arm);
    QCOMPARE(sequence.intents.at(1).action, VoiceCommandAction::Takeoff);
    QCOMPARE(sequence.intents.at(1).parameters.value(QStringLiteral("altitude")).toDouble(), 10.0);
    QCOMPARE(sequence.intents.at(1).targetVehicleId, 42);
    QCOMPARE(matcher.parse(QStringLiteral("arm, and then land")).intents.size(), 2);

    const IntentParseResult motion = matcher.parse(QStringLiteral("move left 5 meters at 2 meters per second"));
    QVERIFY(motion.success);
    QCOMPARE(motion.intents.first().parameters.value(QStringLiteral("direction")).toString(), QStringLiteral("left"));
    QCOMPARE(motion.intents.first().parameters.value(QStringLiteral("distance")).toDouble(), 5.0);
    QCOMPARE(motion.intents.first().parameters.value(QStringLiteral("speed")).toDouble(), 2.0);
    QVERIFY(motion.intents.first().displaySummary.contains(QStringLiteral("2 m/s")));
    QCOMPARE(matcher.parse(QStringLiteral("turn to 270 degrees"))
                 .intents.first()
                 .parameters.value(QStringLiteral("heading"))
                 .toDouble(),
             270.0);
}

void DeterministicMatcherTest::_testContextAndPrimitiveSafety()
{
    DeterministicMatcher matcher;
    VehicleStateSnapshot vehicle;
    vehicle.isArmed = false;
    const IntentParseResult primitive = matcher.parse(QStringLiteral("move forward 10 meters"), vehicle);
    QCOMPARE(primitive.intents.size(), 1);
    QCOMPARE(primitive.intents.first().action, VoiceCommandAction::Move);

    const IntentParseResult launch = matcher.parse(QStringLiteral("launch"), vehicle);
    QCOMPARE(launch.intents.size(), 2);
    QCOMPARE(launch.intents.first().action, VoiceCommandAction::Arm);
    QCOMPARE(launch.intents.last().action, VoiceCommandAction::Takeoff);

    vehicle.isArmed = true;
    QCOMPARE(matcher.parse(QStringLiteral("launch"), vehicle).intents.size(), 1);
    QCOMPARE(matcher.parse(QStringLiteral("launch"), vehicle).intents.first().action, VoiceCommandAction::Takeoff);

    vehicle.isFlying = true;
    QVERIFY(!matcher.parse(QStringLiteral("launch"), vehicle).success);
}

void DeterministicMatcherTest::_testTurkishCommands()
{
    DeterministicMatcher matcher;
    const IntentParseResult sequence = matcher.parse(QStringLiteral("Motorları çalıştır ve kalkış yap 10 metre"));
    QVERIFY(sequence.success);
    QCOMPARE(sequence.intents.size(), 2);
    QCOMPARE(sequence.intents.first().action, VoiceCommandAction::Arm);
    QCOMPARE(sequence.intents.last().action, VoiceCommandAction::Takeoff);
    QCOMPARE(sequence.intents.last().parameters.value(QStringLiteral("altitude")).toDouble(), 10.0);

    const IntentParseResult move = matcher.parse(QStringLiteral("ileri 5 metre git"));
    QVERIFY(move.success);
    QCOMPARE(move.intents.size(), 1);
    QCOMPARE(move.intents.first().action, VoiceCommandAction::Move);
    QCOMPARE(move.intents.first().parameters.value(QStringLiteral("direction")).toString(), QStringLiteral("forward"));
    QCOMPARE(move.intents.first().parameters.value(QStringLiteral("distance")).toDouble(), 5.0);
    for (const auto& [text, action] :
         {QPair{QStringLiteral("90 dereceye dön"), VoiceCommandAction::ChangeHeading},
          QPair{QStringLiteral("irtifayı 10 metreye çıkar"), VoiceCommandAction::ChangeAltitude},
          QPair{QStringLiteral("eve dön"), VoiceCommandAction::ReturnToLaunch}}) {
        const IntentParseResult result = matcher.parse(text);
        QVERIFY2(result.success, qPrintable(text));
        QCOMPARE(result.intents.size(), 1);
        QCOMPARE(result.intents.first().action, action);
    }
}

void DeterministicMatcherTest::_testUnknownAndPartialCommands()
{
    DeterministicMatcher matcher;
    for (const QString& text :
         {QStringLiteral(""), QStringLiteral("arm and dance"), QStringLiteral("move left"),
          QStringLiteral("change heading to 400 degrees"), QStringLiteral("disarm and"), QStringLiteral("please arm"),
          QStringLiteral("take off to 0 meters"), QStringLiteral("move forward 0 meters")}) {
        const IntentParseResult result = matcher.parse(text);
        QVERIFY2(!result.success, qPrintable(text));
        QVERIFY(result.intents.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

UT_REGISTER_TEST(DeterministicMatcherTest, TestLabel::Unit, TestLabel::Utilities)
