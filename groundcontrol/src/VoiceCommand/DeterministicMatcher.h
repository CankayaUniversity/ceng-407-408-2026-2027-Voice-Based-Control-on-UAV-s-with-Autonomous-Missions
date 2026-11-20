#pragma once

#include "IntentParserInterface.h"

/// Matches only complete, supported command clauses; never executes a partial transcript.
class DeterministicMatcher : public IntentParserInterface
{
public:
    IntentParseResult parse(const QString& transcript, const VehicleStateSnapshot& vehicleState = {}) override;

private:
    static MissionIntent makeIntent(VoiceCommandAction action, const QString& summary, int vehicleId,
                                    const QVariantMap& parameters = {});
    static bool matchClause(const QString& clause, const VehicleStateSnapshot& state, QList<MissionIntent>& intents);
};
