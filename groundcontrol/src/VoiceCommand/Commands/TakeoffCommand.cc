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
    if (_dispatched || !checkPreCondition(vehicle)) {
        return false;
    }

    if (_intent.parameters.contains(QStringLiteral("altitude"))) {
        _targetAltitude = _intent.parameters.value(QStringLiteral("altitude")).toDouble();
    } else {
        _targetAltitude = vehicle->minimumTakeoffAltitudeMeters();
    }

    if (!std::isfinite(_targetAltitude) || _targetAltitude <= 0.0) {
        return false;
    }

    dispatched(vehicle);
    vehicle->guidedModeTakeoff(_targetAltitude);

    return true;
}

bool TakeoffCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && vehicle->flying();
}

bool TakeoffCommand::isCompleted(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && vehicle->flying() && vehicle->altitudeRelative()->rawValue().isValid() &&
           vehicle->altitudeRelative()->rawValue().toDouble() >= _targetAltitude - 0.5;
}
