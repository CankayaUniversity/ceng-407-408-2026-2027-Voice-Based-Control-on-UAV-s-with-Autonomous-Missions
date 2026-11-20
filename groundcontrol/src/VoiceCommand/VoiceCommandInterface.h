#pragma once

#include <memory>

#include "MissionIntent.h"

class Vehicle;

/// A single, approved intent. The queue owns the command throughout its execution and
/// checks its post-condition against fresh vehicle telemetry until it succeeds or times out.
/// execute() reports dispatch, not completion; destroying a MOVE command stops its
/// setpoint stream, so the queue must retain it until success or cancellation.
class VoiceCommandInterface
{
public:
    virtual ~VoiceCommandInterface() = default;

    virtual bool checkPreCondition(Vehicle* vehicle) const = 0;
    virtual bool execute(Vehicle* vehicle) = 0;
    virtual bool checkPostCondition(Vehicle* vehicle) const = 0;
};

/// Returns nullptr for unsupported actions or missing/invalid parameters.
/// Does not execute or validate telemetry; the queue checks pre-conditions just in time.
std::unique_ptr<VoiceCommandInterface> makeVoiceCommand(const MissionIntent& intent);
