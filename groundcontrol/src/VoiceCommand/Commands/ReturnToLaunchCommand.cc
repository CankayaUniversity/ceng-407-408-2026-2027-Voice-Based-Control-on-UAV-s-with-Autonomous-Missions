#include "ReturnToLaunchCommand.h"

#include "FirmwarePlugin.h"
#include "Vehicle.h"

ReturnToLaunchCommand::ReturnToLaunchCommand(const MissionIntent& intent) : Command(intent) {}

bool ReturnToLaunchCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && vehicle->flying() &&
           !vehicle->firmwarePlugin()->rtlFlightMode().isEmpty();
}

bool ReturnToLaunchCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    dispatched(vehicle);
    vehicle->guidedModeRTL(false);
    return true;
}

bool ReturnToLaunchCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && vehicle->flightMode() == vehicle->firmwarePlugin()->rtlFlightMode();
}

bool ReturnToLaunchCommand::isCompleted(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && !vehicle->flying();
}
