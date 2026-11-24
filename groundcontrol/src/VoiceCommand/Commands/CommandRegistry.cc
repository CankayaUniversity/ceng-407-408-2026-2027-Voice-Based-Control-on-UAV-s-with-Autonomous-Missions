#include "CommandRegistry.h"

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

template <typename T>
VoiceCommandFactory::Builder makeCommandBuilder()
{
    return [](const MissionIntent& intent) -> std::unique_ptr<VoiceCommandInterface> {
        auto command = std::make_unique<T>(intent);
        return command->hasValidParameters() ? std::move(command) : nullptr;
    };
}

}  // namespace

CommandRegistry::CommandRegistry()
{
    _builders.emplace(VoiceCommandAction::Arm, makeCommandBuilder<ArmCommand>());
    _builders.emplace(VoiceCommandAction::Disarm, makeCommandBuilder<DisarmCommand>());
    _builders.emplace(VoiceCommandAction::Takeoff, makeCommandBuilder<TakeoffCommand>());
    _builders.emplace(VoiceCommandAction::Land, makeCommandBuilder<LandCommand>());
    _builders.emplace(VoiceCommandAction::Hold, makeCommandBuilder<HoldCommand>());
    _builders.emplace(VoiceCommandAction::ReturnToLaunch, makeCommandBuilder<ReturnToLaunchCommand>());
    _builders.emplace(VoiceCommandAction::ChangeAltitude, makeCommandBuilder<ChangeAltitudeCommand>());
    _builders.emplace(VoiceCommandAction::ChangeHeading, makeCommandBuilder<ChangeHeadingCommand>());
    _builders.emplace(VoiceCommandAction::Move, makeCommandBuilder<MoveCommand>());
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
