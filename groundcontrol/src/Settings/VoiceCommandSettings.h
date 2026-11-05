#pragma once

#include <QtQmlIntegration/QtQmlIntegration>

#include "SettingsGroup.h"

/// \brief Voice Command Settings
///
class VoiceCommandSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
public:
    VoiceCommandSettings(QObject* parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(selectedMicrophone)
    DEFINE_SETTINGFACT(noiseFilterEnabled)
    DEFINE_SETTINGFACT(noiseFilterLevel)
    DEFINE_SETTINGFACT(maxRecordingDuration)
    DEFINE_SETTINGFACT(pushToTalkKey)
    DEFINE_SETTINGFACT(sttLanguage)
    DEFINE_SETTINGFACT(sttSelectedModel)
    DEFINE_SETTINGFACT(sttCustomModelPath)
    DEFINE_SETTINGFACT(sttIdleUnloadTimeout)
    DEFINE_SETTINGFACT(sttUseGpu)
};