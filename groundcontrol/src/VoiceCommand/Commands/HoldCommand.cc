#include "HoldCommand.h"

#include "FirmwarePlugin.h"
#include "Vehicle.h"

HoldCommand::HoldCommand(const MissionIntent& intent) : Command(intent) {}

bool HoldCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && vehicle->flying();
}

bool HoldCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    dispatched(vehicle);
    vehicle->pauseVehicle();
    return true;
}

bool HoldCommand::checkPostCondition(Vehicle* vehicle) const
{
    if (!dispatchedTo(vehicle))
        return false;
    const QString mode = vehicle->firmwarePlugin()->pauseFlightMode();
    return mode.isEmpty() || vehicle->flightMode() == mode;
}
