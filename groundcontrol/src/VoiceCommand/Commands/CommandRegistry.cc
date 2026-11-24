#include "CommandRegistry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "ArmCommand.h"
#include "ChangeAltitudeCommand.h"
#include "ChangeHeadingCommand.h"
#include "DisarmCommand.h"
#include "FlyViewSettings.h"
#include "HoldCommand.h"
#include "LandCommand.h"
#include "MoveCommand.h"
#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "ReturnToLaunchCommand.h"
#include "SettingsManager.h"
#include "TakeoffCommand.h"
#include "Vehicle.h"

namespace {

constexpr double kFallbackMaxAltitudeMeters = 120.0;
constexpr double kFallbackMaxMoveDistanceMeters = 100.0;
constexpr double kFallbackMaxMoveSpeedMetersSecond = 5.0;

bool positiveNumberInRange(const QVariant& value, double maxValue)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok && std::isfinite(number) && number > 0.0 && number <= maxValue;
}

bool headingNumber(const QVariant& value)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok && std::isfinite(number) && number >= 0.0 && number <= 360.0;
}

Vehicle* targetVehicle(const MissionIntent& intent)
{
    auto* manager = MultiVehicleManager::instance();
    if (!manager) {
        return nullptr;
    }
    return intent.targetVehicleId != -1 ? manager->getVehicleById(intent.targetVehicleId) : manager->activeVehicle();
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

double tighten(double current, double candidate)
{
    if (!std::isfinite(candidate) || candidate <= 0.0) {
        return current;
    }
    return std::isfinite(current) && current > 0.0 ? std::min(current, candidate) : candidate;
}

double maxAltitudeMeters(const MissionIntent& intent)
{
    double limit = std::numeric_limits<double>::quiet_NaN();
    if (auto* settings = SettingsManager::instance()) {
        if (auto* flyView = settings->flyViewSettings()) {
            limit = tighten(limit, flyView->guidedMaximumAltitude()->rawValue().toDouble());
        }
    }
    limit = tighten(limit, positiveVehicleParam(targetVehicle(intent), QStringLiteral("GF_MAX_VER_DIST")));
    return std::isfinite(limit) ? limit : kFallbackMaxAltitudeMeters;
}

double maxMoveDistanceMeters(const MissionIntent& intent)
{
    double limit = std::numeric_limits<double>::quiet_NaN();
    if (auto* settings = SettingsManager::instance()) {
        if (auto* flyView = settings->flyViewSettings()) {
            limit = tighten(limit, flyView->maxGoToLocationDistance()->rawValue().toDouble());
        }
    }
    limit = tighten(limit, positiveVehicleParam(targetVehicle(intent), QStringLiteral("GF_MAX_HOR_DIST")));
    return std::isfinite(limit) ? limit : kFallbackMaxMoveDistanceMeters;
}

double maxMoveSpeedMetersSecond(const MissionIntent& intent)
{
    if (Vehicle* vehicle = targetVehicle(intent)) {
        const double vehicleLimit = vehicle->maximumHorizontalSpeedMultirotorMetersSecond();
        if (std::isfinite(vehicleLimit) && vehicleLimit > 0.0) {
            return vehicleLimit;
        }
    }
    return kFallbackMaxMoveSpeedMetersSecond;
}

std::unique_ptr<VoiceCommandInterface> makeBuiltinCommand(const MissionIntent& intent)
{
    const auto& p = intent.parameters;
    switch (intent.action) {
        case VoiceCommandAction::Arm:
            return std::make_unique<ArmCommand>(intent);

        case VoiceCommandAction::Disarm:
            return std::make_unique<DisarmCommand>(intent);

        case VoiceCommandAction::Takeoff:
            if (p.contains(QStringLiteral("altitude")) &&
                !positiveNumberInRange(p.value(QStringLiteral("altitude")), maxAltitudeMeters(intent)))
                return {};
            return std::make_unique<TakeoffCommand>(intent);

        case VoiceCommandAction::Land:
            return std::make_unique<LandCommand>(intent);

        case VoiceCommandAction::Hold:
            return std::make_unique<HoldCommand>(intent);

        case VoiceCommandAction::ReturnToLaunch:
            return std::make_unique<ReturnToLaunchCommand>(intent);

        case VoiceCommandAction::ChangeAltitude:
            return positiveNumberInRange(p.value(QStringLiteral("altitude")), maxAltitudeMeters(intent))
                       ? std::make_unique<ChangeAltitudeCommand>(intent)
                       : nullptr;

        case VoiceCommandAction::ChangeHeading:
            return headingNumber(p.value(QStringLiteral("heading"))) ? std::make_unique<ChangeHeadingCommand>(intent)
                                                                     : nullptr;

        case VoiceCommandAction::Move: {
            const QString direction = p.value(QStringLiteral("direction")).toString();

            if (direction != QStringLiteral("forward") && direction != QStringLiteral("backward") &&
                direction != QStringLiteral("left") && direction != QStringLiteral("right") &&
                direction != QStringLiteral("north") && direction != QStringLiteral("south") &&
                direction != QStringLiteral("east") && direction != QStringLiteral("west"))
                return {};
            if (!positiveNumberInRange(p.value(QStringLiteral("distance")), maxMoveDistanceMeters(intent)) ||
                (p.contains(QStringLiteral("speed")) &&
                 !positiveNumberInRange(p.value(QStringLiteral("speed")), maxMoveSpeedMetersSecond(intent))))
                return {};

            return std::make_unique<MoveCommand>(intent);
        }
    }
    return {};
}

}  // namespace

CommandRegistry::CommandRegistry()
{
    for (const VoiceCommandAction action :
         {VoiceCommandAction::Arm, VoiceCommandAction::Disarm, VoiceCommandAction::Takeoff, VoiceCommandAction::Land,
          VoiceCommandAction::Hold, VoiceCommandAction::ReturnToLaunch, VoiceCommandAction::ChangeAltitude,
          VoiceCommandAction::ChangeHeading, VoiceCommandAction::Move}) {
        _builders.emplace(action, [](const MissionIntent& intent) { return makeBuiltinCommand(intent); });
    }
}

CommandRegistry& CommandRegistry::instance()
{
    static CommandRegistry registry;
    return registry;
}

bool CommandRegistry::registerBuilder(VoiceCommandAction action, VoiceCommandFactory::Builder builder)
{
    if (!builder)
        return false;
    QMutexLocker locker(&_mutex);
    return _builders.emplace(action, std::move(builder)).second;
}

VoiceCommandFactory::Builder CommandRegistry::builderFor(VoiceCommandAction action)
{
    QMutexLocker locker(&_mutex);
    const auto it = _builders.find(action);
    return it != _builders.end() ? it->second : VoiceCommandFactory::Builder{};
}
