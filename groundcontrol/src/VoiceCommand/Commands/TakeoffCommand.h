#pragma once

#include "Commands.h"

class TakeoffCommand final : public Command
{
public:
    explicit TakeoffCommand(const MissionIntent& intent);

    bool hasValidParameters() const override;
    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;
};
