#pragma once

#include "UnitTest.h"

class NoiseFilterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _testDefaultAndSetStrength();
    void _testBypassWhenZero();
    void _testHighPassFilterRumbleSuppression();
    void _testNoiseGateSuppression();
    void _testReset();
};
