#pragma once

#include "UnitTest.h"

class AudioLevelMeterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _testInitialState();
    void _testTestToggleLifecycle();
    void _testLevelCalculationFromStream();
    void _testGoertzelFrequencyBands();
    void _testDecayBehavior();
    void _testResetLevels();
};
