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
    void _testCaptureLifecycleWithTestSeam();
    void _testStreamingLifecycleWithTestSeam();
    void _testConflictPreemption();
    void _testDownmixStereoToMono();
    void _testResamplingTo16kHz();
    void _testNoiseFilterSettings();
    void _testNoiseFilterSwapping();
    void _testNoiseFilterProcessingWithInjectedAudio();
    void _testSafetyLimitReached();
    void _testDeviceDisconnectionHandling();
};
