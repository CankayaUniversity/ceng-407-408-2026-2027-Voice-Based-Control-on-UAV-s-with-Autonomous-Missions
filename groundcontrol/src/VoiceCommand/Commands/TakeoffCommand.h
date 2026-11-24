#pragma once

#include "Commands.h"

class TakeoffCommand final : public Command
{
public:
    explicit TakeoffCommand(const MissionIntent& intent);

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;
    bool isCompleted(Vehicle* vehicle) const override;

private:
    double _targetAltitude = 0.0;
    double _targetAltitudeAMSL = 0.0;
};
