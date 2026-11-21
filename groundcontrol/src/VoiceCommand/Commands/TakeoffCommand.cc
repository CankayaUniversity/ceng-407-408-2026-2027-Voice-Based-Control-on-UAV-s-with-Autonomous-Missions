#include "TakeoffCommand.h"

#include <cmath>

#include "Fact.h"
#include "Vehicle.h"

TakeoffCommand::TakeoffCommand(const MissionIntent& intent) : Command(intent) {}

bool TakeoffCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && !vehicle->flying() && vehicle->altitudeAMSL()->rawValue().isValid() &&
           std::isfinite(vehicle->altitudeAMSL()->rawValue().toDouble());
}

bool TakeoffCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    const double altitude = _intent.parameters.contains(QStringLiteral("altitude"))
                                ? _intent.parameters.value(QStringLiteral("altitude")).toDouble()
                                : vehicle->minimumTakeoffAltitudeMeters();
    if (!std::isfinite(altitude) || altitude <= 0.0)
        return false;
    dispatched(vehicle);
    vehicle->guidedModeTakeoff(altitude);
    return true;
}

bool TakeoffCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && vehicle->flying();
}
