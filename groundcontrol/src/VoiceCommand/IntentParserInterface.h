#pragma once

#include "MissionIntent.h"
#include "VehicleStateSnapshot.h"

class IntentParserInterface
{
public:
    virtual ~IntentParserInterface() = default;
    virtual IntentParseResult parse(const QString& transcript, const VehicleStateSnapshot& vehicleState = {}) = 0;
};
