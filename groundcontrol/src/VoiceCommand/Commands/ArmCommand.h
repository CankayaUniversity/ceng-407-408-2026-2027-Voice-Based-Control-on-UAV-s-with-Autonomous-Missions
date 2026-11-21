#pragma once

#include "Commands.h"

class ArmCommand final : public Command
{
public:
    explicit ArmCommand(const MissionIntent& intent);

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;
};
