#pragma once

#include "VehicleTest.h"

class VoiceCommandQueueRunnerTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void init() override;
    void _testOrderedHandoff();
    void _testJustInTimePreConditionFailure();
    void _testPostConditionTimeout();
    void _testCompletionTimeout();
    void _testAsynchronousFailure();
    void _testCompletedPostConditionStopsWatchdog();
    void _testCancelStopsSequence();
};
