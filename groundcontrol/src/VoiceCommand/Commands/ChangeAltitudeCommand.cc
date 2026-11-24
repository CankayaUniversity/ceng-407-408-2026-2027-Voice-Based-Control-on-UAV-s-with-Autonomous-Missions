#include "ChangeAltitudeCommand.h"

#include <cmath>

#include "Fact.h"
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
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    _target = _intent.parameters.value(QStringLiteral("altitude")).toDouble();
    const double difference = _target - vehicle->altitudeRelative()->rawValue().toDouble();
    dispatched(vehicle);
    if (std::abs(difference) > 0.5)
        vehicle->guidedModeChangeAltitude(difference, false);
    return true;
}

bool ChangeAltitudeCommand::checkPostCondition(Vehicle* vehicle) const
{
    if (!dispatchedTo(vehicle) || !vehicle->altitudeRelative()->rawValue().isValid())
        return false;
    const double altitude = vehicle->altitudeRelative()->rawValue().toDouble();
    return std::isfinite(altitude) && std::abs(altitude - _target) <= 0.5;
}
