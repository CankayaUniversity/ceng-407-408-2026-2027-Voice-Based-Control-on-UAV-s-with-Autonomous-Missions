#pragma once

#include "VehicleTest.h"

class VoiceCommandInterfaceTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void init() override;
    void _testFactory();
    void _testFutureActionRegistration();
    void _testArmDisarmTransition();
    void _testJustInTimePreconditions();
    void _testAltitude();
    void _testRejectedAltitudeAck();
    void _testHeadingAndMove();
    void _testRejectedHeadingAck();
    void _testFlightModeActions();
};
