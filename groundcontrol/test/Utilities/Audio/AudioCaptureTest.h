#pragma once

#include "UnitTest.h"

class AudioCaptureTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _testInitialState();
    void _testTargetFormat();
    void _testCaptureLifecycle();
    void _testStreamingLifecycle();
    void _testNoiseFilterSettings();
    void _testNoiseFilterSwapping();
    void _testSafetyLimitReached();
    void _testStreamingFrameEmission();
    void _testSignalEmissions();
    void _testDeviceDisconnectionHandling();
};
