#pragma once

#include <functional>
#include <memory>

#include "VoiceCommandInterface.h"

/// Built-in Phase 1 commands plus externally registered future actions.
class VoiceCommandFactory
{
public:
    using Builder = std::function<std::unique_ptr<VoiceCommandInterface>(const MissionIntent&)>;

    /// Does not replace an existing action. Register before creating the corresponding command.
    static bool registerBuilder(VoiceCommandAction action, Builder builder);
    /// Returns nullptr for unsupported actions (with a diagnostic) or invalid parameters.
    static std::unique_ptr<VoiceCommandInterface> create(const MissionIntent& intent);
};
