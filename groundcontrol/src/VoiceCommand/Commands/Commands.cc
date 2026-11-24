#include "Commands.h"

#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "VehicleSupports.h"

Command::~Command()
{
    QObject::disconnect(_ackConnection);
}

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

void Command::watchForAck(Vehicle* vehicle, MAV_CMD command)
{
    if (!vehicle || command == MAV_CMD_ENUM_END) {
        return;
    }

    _awaitingAck = true;
    const int component = vehicle->defaultComponentId();
    _ackConnection = QObject::connect(
        vehicle, &Vehicle::mavCommandResult, vehicle,
        [this, vehicle, component, command](int vehicleId, int targetComponent, int resultCommand, int result,
                                            int failureCode) {
            if (vehicleId != vehicle->id() || targetComponent != component || resultCommand != command) {
                return;
            }
            _ackAccepted = result == MAV_RESULT_ACCEPTED && failureCode == Vehicle::MavCmdResultCommandResultOnly;
            _ackFailed = !_ackAccepted;
            QObject::disconnect(_ackConnection);
        });
}

bool Command::acknowledgedBy(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && (!_awaitingAck || _ackAccepted);
}

bool Command::isFailed(Vehicle* vehicle) const
{
    return vehicle && _dispatched && _vehicle == vehicle && _ackFailed;
}
