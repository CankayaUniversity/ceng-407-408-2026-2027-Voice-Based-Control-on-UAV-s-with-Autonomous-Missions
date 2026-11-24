#include "Commands.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "VehicleSupports.h"

namespace CommandHelpers {

bool positiveNumberInRange(const QVariant& value, double maxValue)
{
    bool ok = false;
    const double number = value.toDouble(&ok);

    return ok && std::isfinite(number) && number > 0.0 && number <= maxValue;
}

double positiveVehicleParam(Vehicle* vehicle, const QString& name)
{
    if (!vehicle || !vehicle->parameterManager() ||
        !vehicle->parameterManager()->parameterExists(ParameterManager::defaultComponentId, name)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double value =
        vehicle->parameterManager()->getParameter(ParameterManager::defaultComponentId, name)->rawValue().toDouble();

    return std::isfinite(value) && value > 0.0 ? value : std::numeric_limits<double>::quiet_NaN();
}

double tightenLimit(double current, double candidate)
{
    if (!std::isfinite(candidate) || candidate <= 0.0) {
        return current;
    }

    return std::isfinite(current) && current > 0.0 ? std::min(current, candidate) : candidate;
}

}  // namespace CommandHelpers

Vehicle* Command::targetVehicle() const
{
    auto* manager = MultiVehicleManager::instance();

    if (!manager) {
        return nullptr;
    }

    return _intent.targetVehicleId != -1 ? manager->getVehicleById(_intent.targetVehicleId) : manager->activeVehicle();
}

bool Command::available(Vehicle* vehicle) const
{
    return vehicle && (_intent.targetVehicleId == -1 || _intent.targetVehicleId == vehicle->id()) &&
           vehicle->vehicleLinkManager() && vehicle->vehicleLinkManager()->primaryLink().lock();
}

bool Command::guided(Vehicle* vehicle) const
{
    return available(vehicle) && vehicle->supports() && vehicle->supports()->guidedMode() && vehicle->firmwarePlugin();
}

bool Command::dispatchedTo(Vehicle* vehicle) const
{
    return _dispatched && _vehicle == vehicle && available(vehicle);
}

void Command::dispatched(Vehicle* vehicle)
{
    _vehicle = vehicle;
    _dispatched = true;
}
