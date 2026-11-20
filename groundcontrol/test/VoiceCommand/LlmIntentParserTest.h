#pragma once

#include "UnitTest.h"

class LlmIntentParserTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRequestAndSequence();
    void _testSupportedActions();
    void _testRejectsInvalidCommands();
    void _testNetworkFailure();
};
