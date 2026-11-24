#include "ArmCommand.h"

#include "Vehicle.h"

ArmCommand::ArmCommand(const MissionIntent& intent) : Command(intent) {}

bool ArmCommand::checkPreCondition(Vehicle* vehicle) const
{
    return available(vehicle) && !vehicle->armed() && !vehicle->flying();
}

bool ArmCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle)) {
        return false;
    }

    dispatched(vehicle);

    vehicle->setArmedShowError(true);
    return true;
}

bool ArmCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && vehicle->armed();
}
