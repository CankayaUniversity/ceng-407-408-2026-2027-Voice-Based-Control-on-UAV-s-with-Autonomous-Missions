#include "VoiceCommandSettings.h"

DECLARE_SETTINGGROUP(VoiceCommand, "VoiceCommand") {}

DECLARE_SETTINGSFACT(VoiceCommandSettings, selectedMicrophone)
DECLARE_SETTINGSFACT(VoiceCommandSettings, noiseFilterStrength)
DECLARE_SETTINGSFACT(VoiceCommandSettings, maxRecordingDuration)