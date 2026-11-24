#pragma once

#include "Commands.h"

class ChangeAltitudeCommand final : public Command
{
public:
    explicit ChangeAltitudeCommand(const MissionIntent& intent);

    bool hasValidParameters() const override;
    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;

private:
    double _target = 0.0;
};
