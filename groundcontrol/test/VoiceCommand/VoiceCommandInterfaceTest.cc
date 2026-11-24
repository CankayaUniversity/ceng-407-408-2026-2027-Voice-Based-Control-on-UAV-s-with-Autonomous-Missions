#include "VoiceCommandInterfaceTest.h"

#include <algorithm>
#include <cmath>

#include "Commands/VoiceCommandFactory.h"
#include "Commands/VoiceCommandInterface.h"
#include "DeterministicMatcher.h"
#include "Fact.h"
#include "FirmwarePlugin.h"
#include "FlyViewSettings.h"
#include "SettingsManager.h"
#include "Vehicle.h"

namespace {

class FutureCommand final : public VoiceCommandInterface
{
public:
    bool checkPreCondition(Vehicle* vehicle) const override { return vehicle != nullptr; }

    bool execute(Vehicle* vehicle) override { return vehicle != nullptr; }

    bool checkPostCondition(Vehicle* vehicle) const override { return vehicle != nullptr; }
};

MissionIntent makeIntent(VoiceCommandAction action, QVariantMap parameters = {}, int targetVehicleId = -1)
{
    MissionIntent intent;
    intent.action = action;
    intent.parameters = std::move(parameters);
    intent.targetVehicleId = targetVehicleId;
    return intent;
}

}  // namespace

void VoiceCommandInterfaceTest::init()
{
    VehicleTest::init();
    ignoreLogMessage("ComponentInformation.CompInfoGeneral", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Metadata json schema validation failed:")));
    ignoreLogMessage("ComponentInformation.CompInfoParam", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Metadata json schema validation failed:")));
    ignoreLogMessage(
        "Utilities.QGCSensors", QtWarningMsg,
        QRegularExpression(QStringLiteral("Failed to connect to pressure backend|Error Initializing Pressure Sensor")));
}

void VoiceCommandInterfaceTest::_testFactory()
{
    DeterministicMatcher parser;
    for (const QString& text :
         {QStringLiteral("arm"), QStringLiteral("disarm"), QStringLiteral("take off"), QStringLiteral("land"),
          QStringLiteral("hold"), QStringLiteral("rtl"), QStringLiteral("change altitude to 10 meters"),
          QStringLiteral("turn to 90 degrees"), QStringLiteral("move forward 5 meters")}) {
        const auto parsed = parser.parse(text);
        QVERIFY2(parsed.success, qPrintable(text));
        QCOMPARE(parsed.intents.size(), 1);
        QVERIFY2(makeVoiceCommand(parsed.intents.first()) != nullptr, qPrintable(text));
    }

    MissionIntent invalid;
    invalid.action = VoiceCommandAction::Move;
    invalid.parameters = {{QStringLiteral("direction"), QStringLiteral("forward")},
                          {QStringLiteral("distance"), -10.0}};
    QVERIFY(!makeVoiceCommand(invalid));
    invalid.parameters[QStringLiteral("distance")] = 5.0;
    invalid.parameters[QStringLiteral("speed")] = 0.0;
    QVERIFY(!makeVoiceCommand(invalid));
    invalid.parameters[QStringLiteral("speed")] = 1.0;
    invalid.parameters[QStringLiteral("direction")] = QStringLiteral("up");
    QVERIFY(!makeVoiceCommand(invalid));
    invalid.action = VoiceCommandAction::ChangeHeading;
    invalid.parameters = {{QStringLiteral("heading"), 361.0}};
    QVERIFY(!makeVoiceCommand(invalid));
    invalid.action = VoiceCommandAction::ChangeAltitude;
    invalid.parameters = {};
    QVERIFY(!makeVoiceCommand(invalid));
    invalid.parameters = {{QStringLiteral("altitude"), 5000.0}};
    QVERIFY(!makeVoiceCommand(invalid));

    const double maxAltitude =
        SettingsManager::instance()->flyViewSettings()->guidedMaximumAltitude()->rawValue().toDouble();
    invalid.action = VoiceCommandAction::Takeoff;
    invalid.parameters = {{QStringLiteral("altitude"), maxAltitude}};
    QVERIFY(makeVoiceCommand(invalid) != nullptr);
    invalid.parameters = {{QStringLiteral("altitude"), maxAltitude + 1.0}};
    QVERIFY(!makeVoiceCommand(invalid));

    invalid.action = VoiceCommandAction::Move;
    const double maxDistance =
        SettingsManager::instance()->flyViewSettings()->maxGoToLocationDistance()->rawValue().toDouble();
    invalid.parameters = {{QStringLiteral("direction"), QStringLiteral("forward")},
                          {QStringLiteral("distance"), maxDistance}};
    QVERIFY(makeVoiceCommand(invalid) != nullptr);
    invalid.parameters[QStringLiteral("distance")] = maxDistance + 1.0;
    QVERIFY(!makeVoiceCommand(invalid));
    const double vehicleLimit = vehicle()->maximumHorizontalSpeedMultirotorMetersSecond();
    const double maxSpeed = std::isfinite(vehicleLimit) && vehicleLimit > 0.0 ? vehicleLimit : 5.0;
    invalid.parameters[QStringLiteral("distance")] = 5.0;
    invalid.parameters[QStringLiteral("speed")] = maxSpeed + 1.0;
    QVERIFY(!makeVoiceCommand(invalid));
}

void VoiceCommandInterfaceTest::_testFutureActionRegistration()
{
    const VoiceCommandAction futureAction = static_cast<VoiceCommandAction>(250);
    const MissionIntent futureIntent = makeIntent(futureAction);
    expectLogMessage("VoiceCommand.Factory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Unsupported voice command action: 250")));
    QVERIFY(!VoiceCommandFactory::create(futureIntent));
    verifyExpectedLogMessage();

    QVERIFY(!VoiceCommandFactory::registerBuilder(futureAction, {}));
    QVERIFY(VoiceCommandFactory::registerBuilder(
        futureAction, [](const MissionIntent&) { return std::make_unique<FutureCommand>(); }));
    QVERIFY(!VoiceCommandFactory::registerBuilder(
        futureAction, [](const MissionIntent&) { return std::make_unique<FutureCommand>(); }));
    auto command = VoiceCommandFactory::create(futureIntent);
    QVERIFY(command);
    QVERIFY(command->checkPreCondition(vehicle()));
    QVERIFY(command->execute(vehicle()));
    QVERIFY(command->checkPostCondition(vehicle()));
}

void VoiceCommandInterfaceTest::_testArmDisarmTransition()
{
    const MissionIntent armIntent = makeIntent(VoiceCommandAction::Arm, {}, vehicle()->id());
    auto arm = makeVoiceCommand(armIntent);
    QVERIFY(!arm->checkPreCondition(nullptr));
    QVERIFY(!arm->checkPostCondition(vehicle()));
    QVERIFY(arm->checkPreCondition(vehicle()));
    QVERIFY(arm->execute(vehicle()));
    QVERIFY(!arm->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(arm->checkPostCondition(vehicle()), 5000);
    QVERIFY(!arm->checkPreCondition(vehicle()));

    auto disarm = makeVoiceCommand(makeIntent(VoiceCommandAction::Disarm, {}, vehicle()->id()));
    QVERIFY(disarm->checkPreCondition(vehicle()));
    QVERIFY(!disarm->checkPostCondition(vehicle()));
    QVERIFY(disarm->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(disarm->checkPostCondition(vehicle()), 5000);
}

void VoiceCommandInterfaceTest::_testJustInTimePreconditions()
{
    auto wrongVehicle = makeVoiceCommand(makeIntent(VoiceCommandAction::Arm, {}, vehicle()->id() + 1));
    QVERIFY(!wrongVehicle->checkPreCondition(vehicle()));
    QVERIFY(!wrongVehicle->execute(vehicle()));

    auto takeoff = makeVoiceCommand(makeIntent(VoiceCommandAction::Takeoff));
    QVERIFY(!takeoff->checkPreCondition(vehicle()));
    QVERIFY(!takeoff->execute(vehicle()));
    QVERIFY(!takeoff->checkPostCondition(vehicle()));

    auto arm = makeVoiceCommand(makeIntent(VoiceCommandAction::Arm));
    QVERIFY(arm->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(arm->checkPostCondition(vehicle()), 5000);
    vehicle()->altitudeAMSL()->setRawValue(100.0);
    QVERIFY(takeoff->checkPreCondition(vehicle()));
    QVERIFY(!takeoff->checkPostCondition(vehicle()));

    auto move = makeVoiceCommand(makeIntent(
        VoiceCommandAction::Move,
        {{QStringLiteral("direction"), QStringLiteral("forward")}, {QStringLiteral("distance"), 5.0}}));
    QVERIFY(!move->checkPreCondition(vehicle()));
    QVERIFY(!move->execute(vehicle()));
}

void VoiceCommandInterfaceTest::_testAltitude()
{
    auto arm = makeVoiceCommand(makeIntent(VoiceCommandAction::Arm));
    QVERIFY(arm->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(arm->checkPostCondition(vehicle()), 5000);
    vehicle()->_setFlying(true);
    vehicle()->altitudeRelative()->setRawValue(5.0);
    QGeoCoordinate home(47.0, 8.0, 95.0);
    vehicle()->_setHomePosition(home);
    auto altitude = makeVoiceCommand(
        makeIntent(VoiceCommandAction::ChangeAltitude, {{QStringLiteral("altitude"), 10.0}}));
    QVERIFY(altitude->checkPreCondition(vehicle()));
    QVERIFY(altitude->execute(vehicle()));
    QVERIFY(!altitude->checkPostCondition(vehicle()));
    vehicle()->altitudeRelative()->setRawValue(qQNaN());
    QVERIFY(!altitude->checkPostCondition(vehicle()));
    vehicle()->altitudeRelative()->setRawValue(10.0);
    QVERIFY(altitude->checkPostCondition(vehicle()));
}

void VoiceCommandInterfaceTest::_testHeadingAndMove()
{
    auto arm = makeVoiceCommand(makeIntent(VoiceCommandAction::Arm));
    QVERIFY(arm->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(arm->checkPostCondition(vehicle()), 5000);
    // Have MockLink report a sustained airborne state rather than only setting
    // Vehicle's flag (its next EXTENDED_SYS_STATE would otherwise reset it).
    vehicle()->sendMavCommand(vehicle()->defaultComponentId(), MAV_CMD_NAV_TAKEOFF, false, 0, 0, 0, 0, 0, 0, 5);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle()->flying(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle()->coordinate().isValid(), 10000);
    auto heading = makeVoiceCommand(
        makeIntent(VoiceCommandAction::ChangeHeading, {{QStringLiteral("heading"), 90.0}}));
    const QVariant originalHeading = vehicle()->heading()->rawValue();
    vehicle()->heading()->setRawValue(qQNaN());
    QVERIFY(!heading->checkPreCondition(vehicle()));
    vehicle()->heading()->setRawValue(originalHeading);
    QVERIFY(heading->checkPreCondition(vehicle()));
    // MockLink acknowledges DO_REPOSITION as unsupported; the reported heading
    // telemetry below tests the contract independently of its command support.
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QStringLiteral("Reposition .* command not supported")));
    QVERIFY(heading->execute(vehicle()));
    vehicle()->heading()->setRawValue(90.0);
    QVERIFY(heading->checkPostCondition(vehicle()));

    auto move = makeVoiceCommand(makeIntent(VoiceCommandAction::Move,
                                            {{QStringLiteral("direction"), QStringLiteral("north")},
                                             {QStringLiteral("distance"), 5.0},
                                             {QStringLiteral("speed"), 2.0}}));
    QVERIFY(move->checkPreCondition(vehicle()));
    mockLink()->clearReceivedMavlinkMessageCounts();
    QVERIFY(move->execute(vehicle()));
    QVERIFY(!move->execute(vehicle()));
    QVERIFY(!move->checkPostCondition(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(mockLink()->receivedMavlinkMessageCount(MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED) >= 3,
                             1000);
    mavlink_message_t message{};
    QVERIFY(mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED, message));
    mavlink_set_position_target_local_ned_t setpoint{};
    mavlink_msg_set_position_target_local_ned_decode(&message, &setpoint);
    QCOMPARE(setpoint.target_system, vehicle()->id());
    QCOMPARE(setpoint.coordinate_frame, static_cast<uint8_t>(MAV_FRAME_LOCAL_NED));
    QCOMPARE(setpoint.type_mask, static_cast<uint16_t>(0x0DC7));
    QVERIFY(std::abs(setpoint.vx - 2.0f) < 0.01f);
    QVERIFY(std::abs(setpoint.vy) < 0.01f);
    move.reset();

    auto defaultMove = makeVoiceCommand(makeIntent(
        VoiceCommandAction::Move,
        {{QStringLiteral("direction"), QStringLiteral("north")}, {QStringLiteral("distance"), 5.0}}));
    QVERIFY(defaultMove->checkPreCondition(vehicle()));
    mockLink()->clearReceivedMavlinkMessageCounts();
    QVERIFY(defaultMove->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(
        mockLink()->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED, message), 1000);
    mavlink_msg_set_position_target_local_ned_decode(&message, &setpoint);
    const double cruise = vehicle()->defaultCruiseSpeed();
    const double vehicleLimit = vehicle()->maximumHorizontalSpeedMultirotorMetersSecond();
    const double expectedSpeed =
        std::min(std::isfinite(cruise) && cruise > 0.0 ? cruise : 1.0,
                 std::isfinite(vehicleLimit) && vehicleLimit > 0.0 ? vehicleLimit : 5.0);
    QVERIFY(std::abs(setpoint.vx - expectedSpeed) < 0.01);
    const double overVehicleLimit = (std::isfinite(vehicleLimit) && vehicleLimit > 0.0 ? vehicleLimit : 5.0) + 1.0;
    auto tooFastForVehicle = makeVoiceCommand(makeIntent(VoiceCommandAction::Move,
                                                         {{QStringLiteral("direction"), QStringLiteral("north")},
                                                          {QStringLiteral("distance"), 5.0},
                                                          {QStringLiteral("speed"), overVehicleLimit}}));
    QVERIFY(!tooFastForVehicle);
}

void VoiceCommandInterfaceTest::_testFlightModeActions()
{
    auto arm = makeVoiceCommand(makeIntent(VoiceCommandAction::Arm));
    QVERIFY(arm->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(arm->checkPostCondition(vehicle()), 5000);
    vehicle()->altitudeAMSL()->setRawValue(100.0);
    auto takeoff = makeVoiceCommand(makeIntent(VoiceCommandAction::Takeoff));
    QVERIFY(takeoff->execute(vehicle()));
    QVERIFY(!takeoff->checkPostCondition(vehicle()));
    vehicle()->_setFlying(true);
    QVERIFY(takeoff->checkPostCondition(vehicle()));

    auto rtl = makeVoiceCommand(makeIntent(VoiceCommandAction::ReturnToLaunch));
    QVERIFY(rtl->checkPreCondition(vehicle()));
    QVERIFY(rtl->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(rtl->checkPostCondition(vehicle()), 5000);

    auto land = makeVoiceCommand(makeIntent(VoiceCommandAction::Land));
    QVERIFY(land->execute(vehicle()));
    QTRY_VERIFY_WITH_TIMEOUT(land->checkPostCondition(vehicle()), 5000);

    // MockLink does not implement PX4's reposition command used by pauseVehicle().
    // Simulate the resulting flight-mode telemetry to exercise the post-condition.
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QStringLiteral("Reposition .* command not supported")));
    auto hold = makeVoiceCommand(makeIntent(VoiceCommandAction::Hold));
    QVERIFY(hold->execute(vehicle()));
    QVERIFY(!hold->checkPostCondition(vehicle()));
    vehicle()->setFlightMode(vehicle()->firmwarePlugin()->pauseFlightMode());
    QTRY_VERIFY_WITH_TIMEOUT(hold->checkPostCondition(vehicle()), 5000);
}

UT_REGISTER_TEST(VoiceCommandInterfaceTest, TestLabel::Unit, TestLabel::Vehicle)
