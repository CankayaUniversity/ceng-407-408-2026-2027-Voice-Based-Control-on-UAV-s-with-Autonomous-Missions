#pragma once

#include "Commands.h"

class HoldCommand final : public Command
{
public:
    using Command::Command;

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;
};
