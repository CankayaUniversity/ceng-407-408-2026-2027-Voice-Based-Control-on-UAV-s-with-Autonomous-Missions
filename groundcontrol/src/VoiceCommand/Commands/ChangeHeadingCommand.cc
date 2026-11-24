#include "ChangeHeadingCommand.h"

#include <cmath>

#include "Fact.h"
#include "FirmwarePlugin.h"
#include "Vehicle.h"

ChangeHeadingCommand::ChangeHeadingCommand(const MissionIntent& intent) : Command(intent) {}

bool ChangeHeadingCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && vehicle->flying() && vehicle->coordinate().isValid() &&
           vehicle->heading()->rawValue().isValid();
}

bool ChangeHeadingCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle)) {
        return false;
    }

    _target = _intent.parameters.value(QStringLiteral("heading")).toDouble();
    const MAV_CMD ackCommand = vehicle->firmwarePlugin()->guidedModeChangeHeadingAckCommand();
    if (ackCommand != MAV_CMD_ENUM_END && vehicle->isMavCommandPending(vehicle->defaultComponentId(), ackCommand)) {
        return false;
    }

    dispatched(vehicle);

    watchForAck(vehicle, ackCommand);
    vehicle->guidedModeChangeHeading(vehicle->coordinate().atDistanceAndAzimuth(10.0, _target));

    return true;
}

bool ChangeHeadingCommand::checkPostCondition(Vehicle* vehicle) const
{
    return acknowledgedBy(vehicle);
}

bool ChangeHeadingCommand::isCompleted(Vehicle* vehicle) const
{
    if (!dispatchedTo(vehicle)) {
        return false;
    }

    const double heading = vehicle->heading()->rawValue().toDouble();

    return std::isfinite(heading) && std::abs(std::remainder(heading - _target, 360.0)) <= 5.0;
}
