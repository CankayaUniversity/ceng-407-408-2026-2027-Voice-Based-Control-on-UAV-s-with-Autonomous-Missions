#pragma once

#include "UnitTest.h"

class DspNoiseFilterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _testDefaultAndSetLevel();
    void _testHighPassFilterRumbleSuppression();
    void _testNoiseGateSuppression();
    void _testReset();
};
