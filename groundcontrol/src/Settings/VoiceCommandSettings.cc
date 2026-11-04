#include "VoiceCommandSettings.h"

DECLARE_SETTINGGROUP(VoiceCommand, "VoiceCommand") {}

DECLARE_SETTINGSFACT(VoiceCommandSettings, selectedMicrophone)
DECLARE_SETTINGSFACT(VoiceCommandSettings, noiseFilterEnabled)
DECLARE_SETTINGSFACT(VoiceCommandSettings, noiseFilterLevel)
DECLARE_SETTINGSFACT(VoiceCommandSettings, maxRecordingDuration)