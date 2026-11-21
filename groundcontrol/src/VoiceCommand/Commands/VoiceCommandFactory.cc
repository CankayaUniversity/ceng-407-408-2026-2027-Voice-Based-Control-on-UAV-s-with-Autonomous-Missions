#include "VoiceCommandFactory.h"

#include <QtCore/QLoggingCategory>
#include <utility>

#include "CommandRegistry.h"

Q_LOGGING_CATEGORY(VoiceCommandFactoryLog, "VoiceCommand.Factory")

bool VoiceCommandFactory::registerBuilder(VoiceCommandAction action, Builder builder)
{
    return CommandRegistry::instance().registerBuilder(action, std::move(builder));
}

std::unique_ptr<VoiceCommandInterface> VoiceCommandFactory::create(const MissionIntent& intent)
{
    const Builder builder = CommandRegistry::instance().builderFor(intent.action);
    if (!builder) {
        qCWarning(VoiceCommandFactoryLog) << "Unsupported voice command action:" << static_cast<int>(intent.action);
        return {};
    }
    return builder(intent);
}
