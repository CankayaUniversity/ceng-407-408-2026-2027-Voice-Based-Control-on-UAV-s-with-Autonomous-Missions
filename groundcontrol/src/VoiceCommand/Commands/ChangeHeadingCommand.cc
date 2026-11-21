#include "ChangeHeadingCommand.h"

#include <cmath>

#include "Fact.h"
#include "Vehicle.h"

bool ChangeHeadingCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && vehicle->flying() && vehicle->coordinate().isValid() &&
           vehicle->heading()->rawValue().isValid();
}

bool ChangeHeadingCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    _target = _intent.parameters.value(QStringLiteral("heading")).toDouble();
    dispatched(vehicle);
    vehicle->guidedModeChangeHeading(vehicle->coordinate().atDistanceAndAzimuth(10.0, _target));
    return true;
}

bool ChangeHeadingCommand::checkPostCondition(Vehicle* vehicle) const
{
    if (!dispatchedTo(vehicle))
        return false;
    const double heading = vehicle->heading()->rawValue().toDouble();
    return std::isfinite(heading) && std::abs(std::remainder(heading - _target, 360.0)) <= 5.0;
}
