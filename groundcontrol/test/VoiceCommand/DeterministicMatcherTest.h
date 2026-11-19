#pragma once

#include "UnitTest.h"

class DeterministicMatcherTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testCoreActions();
    void _testSlotsAndSequence();
    void _testContextAndPrimitiveSafety();
    void _testTurkishCommands();
    void _testUnknownAndPartialCommands();
};
