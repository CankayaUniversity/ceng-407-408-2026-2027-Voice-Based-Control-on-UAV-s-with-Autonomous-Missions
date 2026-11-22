#include "VoiceCommandQueueRunnerTest.h"

#include <QtTest/QSignalSpy>
#include <functional>
#include <memory>
#include <utility>

#include "VoiceCommandQueueRunner.h"

namespace {

class StubCommand final : public VoiceCommandInterface
{
public:
    using Check = std::function<bool()>;

    StubCommand(Check pre, Check execute, Check post, Check completed)
        : _pre(std::move(pre)), _execute(std::move(execute)), _post(std::move(post)), _completed(std::move(completed))
    {}

    bool checkPreCondition(Vehicle* vehicle) const override { return vehicle && _pre(); }

    bool execute(Vehicle* vehicle) override { return vehicle && _execute(); }

    bool checkPostCondition(Vehicle* vehicle) const override { return vehicle && _post(); }

    bool isCompleted(Vehicle* vehicle) const override { return vehicle && _completed(); }

private:
    Check _pre;
    Check _execute;
    Check _post;
    Check _completed;
};

MissionIntent intent(VoiceCommandAction action)
{
    MissionIntent result;
    result.action = action;
    result.displaySummary = QStringLiteral("Test step");
    return result;
}

}  // namespace

void VoiceCommandQueueRunnerTest::init()
{
    VehicleTest::init();
    ignoreLogMessage("ComponentInformation.CompInfoGeneral", QtWarningMsg,
                     QRegularExpression(QStringLiteral(
                         "Failed to open schema resource: :/json/component_metadata/general.schema.json")));
    ignoreLogMessage("ComponentInformation.CompInfoParam", QtWarningMsg,
                     QRegularExpression(QStringLiteral(
                         "Failed to open schema resource: :/json/component_metadata/parameter.schema.json")));
}

void VoiceCommandQueueRunnerTest::_testOrderedHandoff()
{
    VoiceCommandQueueRunner runner;
    bool armed = false;
    bool flying = false;
    bool reachedAltitude = false;
    bool moveAccepted = false;
    bool moveCompleted = false;
    QList<VoiceCommandAction> dispatched;
    runner.setCommandBuilder([&](const MissionIntent& step) -> std::unique_ptr<VoiceCommandInterface> {
        switch (step.action) {
            case VoiceCommandAction::Arm:
                return std::make_unique<StubCommand>([&]() { return !armed; },
                                                     [&]() {
                                                         dispatched.append(VoiceCommandAction::Arm);
                                                         armed = true;
                                                         return true;
                                                     },
                                                     [&]() { return armed; }, []() { return true; });
            case VoiceCommandAction::Takeoff:
                return std::make_unique<StubCommand>([&]() { return armed && !flying; },
                                                     [&]() {
                                                         dispatched.append(VoiceCommandAction::Takeoff);
                                                         flying = true;
                                                         return true;
                                                     },
                                                     [&]() { return flying; }, [&]() { return reachedAltitude; });
            case VoiceCommandAction::Move:
                return std::make_unique<StubCommand>([&]() { return armed && flying && reachedAltitude; },
                                                     [&]() {
                                                         dispatched.append(VoiceCommandAction::Move);
                                                         return true;
                                                     },
                                                     [&]() { return moveAccepted; }, [&]() { return moveCompleted; });
            default:
                return nullptr;
        }
    });

    QSignalSpy finished(&runner, &VoiceCommandQueueRunner::sequenceFinished);
    QSignalSpy failed(&runner, &VoiceCommandQueueRunner::sequenceFailed);
    runner.start(
        {intent(VoiceCommandAction::Arm), intent(VoiceCommandAction::Takeoff), intent(VoiceCommandAction::Move)},
        vehicle());
    QCOMPARE(dispatched, (QList<VoiceCommandAction>{VoiceCommandAction::Arm}));
    QTRY_COMPARE_WITH_TIMEOUT(dispatched.size(), 2, 1000);
    QCOMPARE(dispatched.at(1), VoiceCommandAction::Takeoff);
    QVERIFY(!finished.wait(200));  // Post-condition passed; still climbing.
    QCOMPARE(dispatched.size(), 2);
    QCOMPARE(finished.count(), 0);

    reachedAltitude = true;
    QTRY_COMPARE_WITH_TIMEOUT(dispatched.size(), 3, 1000);
    QCOMPARE(dispatched.at(2), VoiceCommandAction::Move);
    QVERIFY(!finished.wait(200));  // Neither initiation nor physical movement finished yet.
    QCOMPARE(finished.count(), 0);
    moveAccepted = true;
    QVERIFY(!finished.wait(200));  // Initiated, but distance is not covered.
    QCOMPARE(finished.count(), 0);
    moveCompleted = true;
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(!runner.isRunning());
}

void VoiceCommandQueueRunnerTest::_testJustInTimePreConditionFailure()
{
    VoiceCommandQueueRunner runner;
    bool armed = false;
    int dispatches = 0;
    runner.setCommandBuilder([&](const MissionIntent& step) -> std::unique_ptr<VoiceCommandInterface> {
        return std::make_unique<StubCommand>(
            [&, action = step.action]() { return action == VoiceCommandAction::Arm ? !armed : false; },
            [&]() {
                ++dispatches;
                armed = true;
                return true;
            },
            [&]() { return armed; }, []() { return true; });
    });
    QSignalSpy failed(&runner, &VoiceCommandQueueRunner::sequenceFailed);
    runner.start({intent(VoiceCommandAction::Arm), intent(VoiceCommandAction::Move), intent(VoiceCommandAction::Land)},
                 vehicle());
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 1000);
    QCOMPARE(failed.at(0).at(0).toInt(), 1);
    QCOMPARE(failed.at(0).at(1).toString(), QStringLiteral("Pre-condition of the command is not satisfied"));
    QCOMPARE(dispatches, 1);
    QVERIFY(!runner.isRunning());
}

void VoiceCommandQueueRunnerTest::_testPostConditionTimeout()
{
    VoiceCommandQueueRunner runner;
    int dispatches = 0;
    runner.setCommandBuilder([&](const MissionIntent&) -> std::unique_ptr<VoiceCommandInterface> {
        return std::make_unique<StubCommand>([]() { return true; },
                                             [&]() {
                                                 ++dispatches;
                                                 return true;
                                             },
                                             []() { return false; }, []() { return true; });
    });
    QSignalSpy failed(&runner, &VoiceCommandQueueRunner::sequenceFailed);
    runner.start({intent(VoiceCommandAction::Arm), intent(VoiceCommandAction::Takeoff)}, vehicle());
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 4000);
    QCOMPARE(dispatches, 1);
    QCOMPARE(failed.at(0).at(0).toInt(), 0);
    QVERIFY(!runner.isRunning());
}

void VoiceCommandQueueRunnerTest::_testCompletedPostConditionStopsWatchdog()
{
    VoiceCommandQueueRunner runner;
    bool maneuverComplete = false;
    runner.setCommandBuilder([&](const MissionIntent&) -> std::unique_ptr<VoiceCommandInterface> {
        return std::make_unique<StubCommand>([]() { return true; }, []() { return true; }, []() { return true; },
                                             [&]() { return maneuverComplete; });
    });
    QSignalSpy finished(&runner, &VoiceCommandQueueRunner::sequenceFinished);
    QSignalSpy failed(&runner, &VoiceCommandQueueRunner::sequenceFailed);
    runner.start({intent(VoiceCommandAction::Takeoff)}, vehicle());
    QVERIFY(!failed.wait(3200));
    QCOMPARE(failed.count(), 0);
    QVERIFY(runner.isRunning());
    maneuverComplete = true;
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
    QCOMPARE(failed.count(), 0);
}

void VoiceCommandQueueRunnerTest::_testCancelStopsSequence()
{
    VoiceCommandQueueRunner runner;
    int dispatches = 0;
    runner.setCommandBuilder([&](const MissionIntent&) -> std::unique_ptr<VoiceCommandInterface> {
        return std::make_unique<StubCommand>([]() { return true; },
                                             [&]() {
                                                 ++dispatches;
                                                 return true;
                                             },
                                             []() { return true; }, []() { return false; });
    });
    QSignalSpy finished(&runner, &VoiceCommandQueueRunner::sequenceFinished);
    QSignalSpy failed(&runner, &VoiceCommandQueueRunner::sequenceFailed);
    runner.start({intent(VoiceCommandAction::Takeoff), intent(VoiceCommandAction::Move)}, vehicle());
    QCOMPARE(dispatches, 1);
    runner.cancel();
    QVERIFY(!finished.wait(200));
    QCOMPARE(finished.count(), 0);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(dispatches, 1);
    QVERIFY(!runner.isRunning());
}

UT_REGISTER_TEST(VoiceCommandQueueRunnerTest, TestLabel::Integration, TestLabel::Vehicle)
