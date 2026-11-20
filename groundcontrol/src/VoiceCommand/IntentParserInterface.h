#pragma once

#include <functional>

#include "MissionIntent.h"

class IntentParserInterface
{
public:
    virtual ~IntentParserInterface() = default;
    using Completion = std::function<void(IntentParseResult)>;
    virtual void parse(const QString& transcript, const VehicleStateSnapshot& vehicleState, Completion completion) = 0;
};
