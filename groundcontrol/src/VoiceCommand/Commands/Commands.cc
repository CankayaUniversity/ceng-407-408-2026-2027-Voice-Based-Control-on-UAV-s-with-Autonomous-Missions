#include "Commands.h"

#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "VehicleSupports.h"

bool Command::available(Vehicle* vehicle) const
{
    return vehicle && (_intent.targetVehicleId == -1 || _intent.targetVehicleId == vehicle->id()) &&
           vehicle->vehicleLinkManager() && vehicle->vehicleLinkManager()->primaryLink().lock();
}

bool Command::guided(Vehicle* vehicle) const
{
    return available(vehicle) && vehicle->supports() && vehicle->supports()->guidedMode() && vehicle->firmwarePlugin();
}

bool Command::dispatchedTo(Vehicle* vehicle) const
{
    return _dispatched && _vehicle == vehicle && available(vehicle);
}

void Command::dispatched(Vehicle* vehicle)
{
    _vehicle = vehicle;
    _dispatched = true;
}
