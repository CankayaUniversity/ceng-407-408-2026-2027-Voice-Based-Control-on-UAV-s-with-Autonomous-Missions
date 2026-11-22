#pragma once

#include "Commands.h"

class LandCommand final : public Command
{
public:
    explicit LandCommand(const MissionIntent& intent);

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;
    bool isCompleted(Vehicle* vehicle) const override;
};
