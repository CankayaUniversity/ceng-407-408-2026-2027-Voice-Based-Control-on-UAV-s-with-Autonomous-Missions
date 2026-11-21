#include "DisarmCommand.h"

#include "Vehicle.h"

DisarmCommand::DisarmCommand(const MissionIntent& intent) : Command(intent) {}

bool DisarmCommand::checkPreCondition(Vehicle* vehicle) const
{
    return available(vehicle) && vehicle->armed() && !vehicle->flying();
}

bool DisarmCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    dispatched(vehicle);
    vehicle->setArmedShowError(false);
    return true;
}

bool DisarmCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && !vehicle->armed();
}
