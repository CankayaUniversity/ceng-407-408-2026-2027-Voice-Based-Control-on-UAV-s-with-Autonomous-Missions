#include "ChangeAltitudeCommand.h"

#include <cmath>
#include <limits>

#include "Fact.h"
#include "FlyViewSettings.h"
#include "SettingsManager.h"
#include "Vehicle.h"

ChangeAltitudeCommand::ChangeAltitudeCommand(const MissionIntent& intent) : Command(intent) {}

bool ChangeAltitudeCommand::hasValidParameters() const
{
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
    _target = _intent.parameters.value(VoiceCommandParam::altitude()).toDouble();
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
