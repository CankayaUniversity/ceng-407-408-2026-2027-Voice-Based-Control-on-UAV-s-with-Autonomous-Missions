#pragma once

#include "UnitTest.h"

class AudioDeviceManagerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _testInitialState();
    void _testAvailableMicrophones();
    void _testSelectionAndPersistence();
    void _testInvalidDeviceState();
    void _testSignalEmissions();
    void _testSpecs();
};
