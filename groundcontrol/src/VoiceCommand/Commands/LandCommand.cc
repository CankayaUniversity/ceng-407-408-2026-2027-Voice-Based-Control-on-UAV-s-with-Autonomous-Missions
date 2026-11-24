#include "LandCommand.h"

#include "FirmwarePlugin.h"
#include "Vehicle.h"

LandCommand::LandCommand(const MissionIntent& intent) : Command(intent) {}

bool LandCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && vehicle->flying();
}

bool LandCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle)) {
        return false;
    }

    dispatched(vehicle);

    vehicle->guidedModeLand();

    return true;
}

bool LandCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) &&
           (!vehicle->flying() || (!vehicle->firmwarePlugin()->landFlightMode().isEmpty() &&
                                   vehicle->flightMode() == vehicle->firmwarePlugin()->landFlightMode()));
}

bool LandCommand::isCompleted(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && !vehicle->flying();
}
