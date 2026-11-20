#pragma once

#include "IntentParserInterface.h"

/// Matches only complete, supported command clauses; never executes a partial transcript.
class DeterministicMatcher : public IntentParserInterface
{
public:
    IntentParseResult parse(const QString& transcript, const VehicleStateSnapshot& vehicleState = {}) override;
};
