#pragma once

#include "UnitTest.h"

class VoiceCommandManagerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _testInitialState();
    void _testPushToTalkKeyConfiguration();
    void _testRecordingLifecycleDelegation();
    void _testSignalPropagation();
    void _testKeyEventFilter();
    void _testAutoRepeatIgnored();
    void _testVehicleConnectedGating();
    void _testSttModelAvailableGating();

    // Issue #38
    void _testMinimumDurationValidation();

    // Issue #58
    void _testSessionStateMachine();
    void _testPreWarmingOnPttPress();
    void _testIdleUnloadTimer();
    void _testAsynchronousWorkerTranscription();
};
