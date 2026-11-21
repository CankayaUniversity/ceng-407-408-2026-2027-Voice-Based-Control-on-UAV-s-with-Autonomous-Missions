#include "ChangeAltitudeCommand.h"

#include <cmath>

#include "Fact.h"
#include "Vehicle.h"

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
    return dispatchedTo(vehicle) && std::abs(vehicle->altitudeRelative()->rawValue().toDouble() - _target) <= 0.5;
}
