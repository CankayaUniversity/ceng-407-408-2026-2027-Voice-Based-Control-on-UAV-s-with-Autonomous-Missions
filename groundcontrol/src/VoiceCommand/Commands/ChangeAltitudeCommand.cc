#include "ChangeAltitudeCommand.h"

#include <cmath>

#include "Fact.h"
#include "FirmwarePlugin.h"
#include "Vehicle.h"

ChangeAltitudeCommand::ChangeAltitudeCommand(const MissionIntent& intent) : Command(intent) {}

bool ChangeAltitudeCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && vehicle->flying() &&
           vehicle->altitudeRelative()->rawValue().isValid() &&
           std::isfinite(vehicle->altitudeRelative()->rawValue().toDouble()) && vehicle->homePosition().isValid() &&
           std::isfinite(vehicle->homePosition().altitude());
}

bool ChangeAltitudeCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle)) {
        return false;
    }

    _target = _intent.parameters.value(QStringLiteral("altitude")).toDouble();

    const double difference = _target - vehicle->altitudeRelative()->rawValue().toDouble();
    const MAV_CMD ackCommand =
        std::abs(difference) > 0.5 ? vehicle->firmwarePlugin()->guidedModeChangeAltitudeAckCommand() : MAV_CMD_ENUM_END;
    if (ackCommand != MAV_CMD_ENUM_END && vehicle->isMavCommandPending(vehicle->defaultComponentId(), ackCommand)) {
        return false;
    }

    dispatched(vehicle);

    if (std::abs(difference) > 0.5) {
        watchForAck(vehicle, ackCommand);
        vehicle->guidedModeChangeAltitude(difference, false);
    }

    return true;
}

bool ChangeAltitudeCommand::checkPostCondition(Vehicle* vehicle) const
{
    return acknowledgedBy(vehicle);
}

bool ChangeAltitudeCommand::isCompleted(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && std::abs(vehicle->altitudeRelative()->rawValue().toDouble() - _target) <= 0.5;
}
