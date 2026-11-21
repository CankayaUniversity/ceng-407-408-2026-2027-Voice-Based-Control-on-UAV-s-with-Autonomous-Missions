#pragma once

#include "Commands.h"

class DisarmCommand final : public Command
{
public:
    explicit DisarmCommand(const MissionIntent& intent);

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;
};
