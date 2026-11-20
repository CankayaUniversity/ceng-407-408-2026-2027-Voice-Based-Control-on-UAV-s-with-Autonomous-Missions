#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <cstdint>

enum class VoiceCommandAction : std::uint8_t
{
    Arm,
    Disarm,
    Takeoff,
    Land,
    Hold,
    ReturnToLaunch,
    ChangeAltitude,
    ChangeHeading,
    Move
};

struct MissionIntent
{
    Q_GADGET
    Q_PROPERTY(VoiceCommandAction action MEMBER action)
    Q_PROPERTY(QVariantMap parameters MEMBER parameters)
    Q_PROPERTY(int targetVehicleId MEMBER targetVehicleId)
    Q_PROPERTY(QString displaySummary MEMBER displaySummary)

public:
    VoiceCommandAction action = VoiceCommandAction::Hold;
    QVariantMap parameters;
    int targetVehicleId = -1;
    QString displaySummary;

    bool operator==(const MissionIntent&) const = default;
};
Q_DECLARE_METATYPE(MissionIntent)

struct IntentParseResult
{
    Q_GADGET
    Q_PROPERTY(bool success MEMBER success)
    Q_PROPERTY(QString rawTranscript MEMBER rawTranscript)
    Q_PROPERTY(QList<MissionIntent> intents MEMBER intents)
    Q_PROPERTY(QString errorMessage MEMBER errorMessage)

public:
    bool success = false;
    QString rawTranscript;
    QList<MissionIntent> intents;
    QString errorMessage;
};
Q_DECLARE_METATYPE(IntentParseResult)
