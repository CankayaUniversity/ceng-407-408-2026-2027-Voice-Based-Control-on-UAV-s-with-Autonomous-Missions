#include "CommandRegistry.h"

#include <cmath>
#include <utility>

#include "ArmCommand.h"
#include "ChangeAltitudeCommand.h"
#include "ChangeHeadingCommand.h"
#include "DisarmCommand.h"
#include "HoldCommand.h"
#include "LandCommand.h"
#include "MoveCommand.h"
#include "ReturnToLaunchCommand.h"
#include "TakeoffCommand.h"

namespace {

bool positiveNumber(const QVariant& value)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok && std::isfinite(number) && number > 0.0;
}

bool headingNumber(const QVariant& value)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok && std::isfinite(number) && number >= 0.0 && number <= 360.0;
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
            if (p.contains(QStringLiteral("altitude")) && !positiveNumber(p.value(QStringLiteral("altitude"))))
                return {};
            return std::make_unique<TakeoffCommand>(intent);

        case VoiceCommandAction::Land:
            return std::make_unique<LandCommand>(intent);

        case VoiceCommandAction::Hold:
            return std::make_unique<HoldCommand>(intent);

        case VoiceCommandAction::ReturnToLaunch:
            return std::make_unique<ReturnToLaunchCommand>(intent);

        case VoiceCommandAction::ChangeAltitude:
            return positiveNumber(p.value(QStringLiteral("altitude"))) ? std::make_unique<ChangeAltitudeCommand>(intent)
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
            if (!positiveNumber(p.value(QStringLiteral("distance"))) ||
                (p.contains(QStringLiteral("speed")) && !positiveNumber(p.value(QStringLiteral("speed")))))
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
