#include "ChangeHeadingCommand.h"

#include <cmath>

#include "Fact.h"
#include "Vehicle.h"
#include "VehicleSupports.h"

ChangeHeadingCommand::ChangeHeadingCommand(const MissionIntent& intent) : Command(intent) {}

bool ChangeHeadingCommand::hasValidParameters() const
{
    bool ok = false;
    const double heading = _intent.parameters.value(VoiceCommandParam::heading()).toDouble(&ok);
    
    return ok && std::isfinite(heading) && heading >= 0.0 && heading <= 360.0;
}

bool ChangeHeadingCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->supports()->changeHeading() && vehicle->armed() && vehicle->flying() &&
           vehicle->coordinate().isValid() && vehicle->heading()->rawValue().isValid() &&
           std::isfinite(vehicle->heading()->rawValue().toDouble());
}

bool ChangeHeadingCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    _target = _intent.parameters.value(VoiceCommandParam::heading()).toDouble();
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
