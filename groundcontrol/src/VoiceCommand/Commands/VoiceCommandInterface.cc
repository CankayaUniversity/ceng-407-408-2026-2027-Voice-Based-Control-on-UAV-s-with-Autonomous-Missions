#include "VoiceCommandInterface.h"

#include "VoiceCommandFactory.h"

std::unique_ptr<VoiceCommandInterface> makeVoiceCommand(const MissionIntent& intent)
{
    return VoiceCommandFactory::create(intent);
}
