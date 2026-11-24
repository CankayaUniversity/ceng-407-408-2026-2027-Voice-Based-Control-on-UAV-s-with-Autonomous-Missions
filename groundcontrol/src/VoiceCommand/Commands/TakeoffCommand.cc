#include "TakeoffCommand.h"

#include <cmath>
#include <limits>

#include "Fact.h"
#include "FlyViewSettings.h"
#include "SettingsManager.h"
#include "Vehicle.h"

TakeoffCommand::TakeoffCommand(const MissionIntent& intent) : Command(intent) {}

bool TakeoffCommand::hasValidParameters() const
{
    if (!_intent.parameters.contains(VoiceCommandParam::altitude())) {
        return true;
    }
    
    double maxAltitude = std::numeric_limits<double>::quiet_NaN();

    if (auto* settings = SettingsManager::instance()) {
        if (auto* flyView = settings->flyViewSettings()) {
            maxAltitude =
                CommandHelpers::tightenLimit(maxAltitude, flyView->guidedMaximumAltitude()->rawValue().toDouble());
        }
    }

    maxAltitude = CommandHelpers::tightenLimit(
        maxAltitude, CommandHelpers::positiveVehicleParam(targetVehicle(), QStringLiteral("GF_MAX_VER_DIST")));
        
    return CommandHelpers::positiveNumberInRange(_intent.parameters.value(VoiceCommandParam::altitude()),
                                                 std::isfinite(maxAltitude) ? maxAltitude : 120.0);
}

bool TakeoffCommand::checkPreCondition(Vehicle* vehicle) const
{
    return guided(vehicle) && vehicle->armed() && !vehicle->flying() && vehicle->altitudeAMSL()->rawValue().isValid() &&
           std::isfinite(vehicle->altitudeAMSL()->rawValue().toDouble());
}

bool TakeoffCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle))
        return false;
    const double altitude = _intent.parameters.contains(VoiceCommandParam::altitude())
                                ? _intent.parameters.value(VoiceCommandParam::altitude()).toDouble()
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
