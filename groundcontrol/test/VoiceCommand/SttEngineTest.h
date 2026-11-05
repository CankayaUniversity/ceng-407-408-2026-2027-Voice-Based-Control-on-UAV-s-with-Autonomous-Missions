#pragma once

#include "UnitTest.h"

class SttEngineTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testSttConfigResolvedThreadCount();
    void _testEngineFactoryInferType();
    void _testEngineFactoryCreate();
    void _testPcm16ToFloatConversion();
    void _testWhisperEngineLifecycleAndErrorHandling();
    void _testWhisperTranscriptionIfModelAvailable();
};
