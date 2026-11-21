#pragma once

#include "Commands.h"

class ChangeHeadingCommand final : public Command
{
public:
    explicit ChangeHeadingCommand(const MissionIntent& intent);

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;

private:
    double _target = 0.0;
};
