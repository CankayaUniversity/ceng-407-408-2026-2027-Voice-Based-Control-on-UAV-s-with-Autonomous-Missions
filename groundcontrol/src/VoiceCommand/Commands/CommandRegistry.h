#pragma once

#include <QtCore/QMutex>
#include <map>

#include "VoiceCommandFactory.h"

class CommandRegistry
{
public:
    static CommandRegistry& instance();

    bool registerBuilder(VoiceCommandAction action, VoiceCommandFactory::Builder builder);
    VoiceCommandFactory::Builder builderFor(VoiceCommandAction action);

private:
    CommandRegistry();
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    QMutex _mutex;
    std::map<VoiceCommandAction, VoiceCommandFactory::Builder> _builders;
};
