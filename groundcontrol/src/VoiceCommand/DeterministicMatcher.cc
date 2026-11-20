#include "DeterministicMatcher.h"

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

QString DeterministicMatcher::normalizeCommand(const QString& transcript, bool turkishCasing)
{
    static const QRegularExpression trailingPunctuation(QStringLiteral(R"([.!?]+$)"));
    QString command = transcript.trimmed();
    if (turkishCasing) {
        // QString::toLower() is locale-independent: Turkish I -> ı, İ -> i.
        command.replace(QLatin1Char('I'), QChar(0x0131));
        command.replace(QChar(0x0130), QLatin1Char('i'));
    }
    command = command.toLower();
    command.remove(trailingPunctuation);
    return command;
}

MissionIntent DeterministicMatcher::makeIntent(VoiceCommandAction action, const QString& summary, int vehicleId,
                                               const QVariantMap& parameters)
{
    return {.action = action, .parameters = parameters, .targetVehicleId = vehicleId, .displaySummary = summary};
}

bool DeterministicMatcher::matchClause(const QString& clause, const VehicleStateSnapshot& state,
                                       QList<MissionIntent>& intents)
{
    static const QRegularExpression arm(QStringLiteral(R"(^(?:arm|arm the (?:drone|vehicle)|motorları çalıştır)$)"));
    static const QRegularExpression disarm(
        QStringLiteral(R"(^(?:disarm|disarm the (?:drone|vehicle)|motorları durdur)$)"));
    static const QRegularExpression land(QStringLiteral(R"(^(?:land|land the (?:drone|vehicle)|iniş yap)$)"));
    static const QRegularExpression hold(QStringLiteral(R"(^(?:hold|hover|stop|hold position|bekle|havada bekle)$)"));
    static const QRegularExpression rtl(QStringLiteral(R"(^(?:return to launch|return home|go home|rtl|eve dön)$)"));
    static const QRegularExpression launch(QStringLiteral(R"(^(?:launch|start flight|uçuşa başla)$)"));
    static const QRegularExpression takeoff(QStringLiteral(
        R"(^(?:take\s?off|take\s?off to|take\s?off at|kalkış yap)(?:\s+(\d+(?:\.\d+)?)\s*(?:meters?|metres?|m|metre))?$)"));
    static const QRegularExpression altitude(QStringLiteral(
        R"(^(?:change altitude to|climb to|descend to|fly at|irtifayı)\s+(\d+(?:\.\d+)?)\s*(?:meters?|metres?|m|metreye(?:\s+(?:çıkar|indir))?)$)"));
    static const QRegularExpression heading(QStringLiteral(
        R"(^(?:change heading to|turn to|face)\s+(\d+(?:\.\d+)?)\s*(?:degrees?|deg|°)$|^(\d+(?:\.\d+)?)\s*dereceye dön$)"));
    static const QRegularExpression move(QStringLiteral(
        R"(^(?:move|fly)\s+(forward|backward|back|left|right|north|south|east|west)\s+(\d+(?:\.\d+)?)\s*(?:meters?|metres?|m)(?:\s+at\s+(\d+(?:\.\d+)?)\s*(?:meters?|metres?|m)(?:\s+per\s+second)?)?$|^(ileri|geri|sola|sağa|kuzeye|güneye|doğuya|batıya)\s+(\d+(?:\.\d+)?)\s*metre\s+git$)"));

    if (arm.match(clause).hasMatch()) {
        intents.append(makeIntent(VoiceCommandAction::Arm, QStringLiteral("Arm"), state.vehicleId));
    } else if (disarm.match(clause).hasMatch()) {
        intents.append(makeIntent(VoiceCommandAction::Disarm, QStringLiteral("Disarm"), state.vehicleId));
    } else if (land.match(clause).hasMatch()) {
        intents.append(makeIntent(VoiceCommandAction::Land, QStringLiteral("Land"), state.vehicleId));
    } else if (hold.match(clause).hasMatch()) {
        intents.append(makeIntent(VoiceCommandAction::Hold, QStringLiteral("Hold position"), state.vehicleId));
    } else if (rtl.match(clause).hasMatch()) {
        intents.append(
            makeIntent(VoiceCommandAction::ReturnToLaunch, QStringLiteral("Return to launch"), state.vehicleId));
    } else if (launch.match(clause).hasMatch()) {
        // "Launch" is a high-level goal; primitive "take off" never adds an implicit arm step.
        if (state.isFlying) {
            return false;
        }
        if (!state.isArmed) {
            intents.append(makeIntent(VoiceCommandAction::Arm, QStringLiteral("Arm"), state.vehicleId));
        }
        intents.append(makeIntent(VoiceCommandAction::Takeoff, QStringLiteral("Take off"), state.vehicleId));
    } else if (const auto takeoffMatch = takeoff.match(clause); takeoffMatch.hasMatch()) {
        QVariantMap parameters;
        QString summary = QStringLiteral("Take off");
        if (!takeoffMatch.captured(1).isEmpty()) {
            const double altitudeM = takeoffMatch.captured(1).toDouble();
            if (altitudeM <= 0.0) {
                return false;
            }
            parameters.insert(QStringLiteral("altitude"), altitudeM);
            summary += QStringLiteral(" to %1 m").arg(takeoffMatch.captured(1));
        }
        intents.append(makeIntent(VoiceCommandAction::Takeoff, summary, state.vehicleId, parameters));
    } else if (const auto altitudeMatch = altitude.match(clause); altitudeMatch.hasMatch()) {
        const double altitudeM = altitudeMatch.captured(1).toDouble();
        if (altitudeM <= 0.0) {
            return false;
        }
        intents.append(makeIntent(VoiceCommandAction::ChangeAltitude,
                                  QStringLiteral("Change altitude to %1 m").arg(altitudeMatch.captured(1)),
                                  state.vehicleId, {{QStringLiteral("altitude"), altitudeM}}));
    } else if (const auto headingMatch = heading.match(clause); headingMatch.hasMatch()) {
        const double degrees =
            (headingMatch.captured(1).isEmpty() ? headingMatch.captured(2) : headingMatch.captured(1)).toDouble();
        if (degrees > 360.0) {
            return false;
        }
        intents.append(makeIntent(VoiceCommandAction::ChangeHeading,
                                  QStringLiteral("Change heading to %1°").arg(degrees), state.vehicleId,
                                  {{QStringLiteral("heading"), degrees}}));
    } else if (const auto moveMatch = move.match(clause); moveMatch.hasMatch()) {
        static const QHash<QString, QString> translatedDirections{
            {QStringLiteral("back"), QStringLiteral("backward")}, {QStringLiteral("ileri"), QStringLiteral("forward")},
            {QStringLiteral("geri"), QStringLiteral("backward")}, {QStringLiteral("sola"), QStringLiteral("left")},
            {QStringLiteral("sağa"), QStringLiteral("right")},    {QStringLiteral("kuzeye"), QStringLiteral("north")},
            {QStringLiteral("güneye"), QStringLiteral("south")},  {QStringLiteral("doğuya"), QStringLiteral("east")},
            {QStringLiteral("batıya"), QStringLiteral("west")},
        };
        const QString rawDirection = moveMatch.captured(1).isEmpty() ? moveMatch.captured(4) : moveMatch.captured(1);
        const QString direction = translatedDirections.value(rawDirection, rawDirection);
        const double distance =
            (moveMatch.captured(2).isEmpty() ? moveMatch.captured(5) : moveMatch.captured(2)).toDouble();
        if (distance <= 0.0 || (!moveMatch.captured(3).isEmpty() && moveMatch.captured(3).toDouble() <= 0.0)) {
            return false;
        }
        QVariantMap parameters{{QStringLiteral("direction"), direction}, {QStringLiteral("distance"), distance}};
        QString summary = QStringLiteral("Move %1 %2 m").arg(direction).arg(distance);
        if (!moveMatch.captured(3).isEmpty()) {
            parameters.insert(QStringLiteral("speed"), moveMatch.captured(3).toDouble());
            summary += QStringLiteral(" at %1 m/s").arg(moveMatch.captured(3));
        }
        intents.append(makeIntent(VoiceCommandAction::Move, summary, state.vehicleId, parameters));
    } else {
        return false;
    }
    return true;
}

IntentParseResult DeterministicMatcher::parse(const QString& transcript, const VehicleStateSnapshot& vehicleState)
{
    IntentParseResult result;
    result.rawTranscript = transcript;
    if (transcript.trimmed().isEmpty()) {
        result.success = false;
        result.errorMessage = QStringLiteral("No recognized voice command");
        return result;
    }

    static const QRegularExpression separator(
        QStringLiteral(R"(\s*,?\s+(?:and\s+then|and|then|ve\s+sonra|ve|sonra)\s+)"));
    // English ASCII I and Turkish dotted/dotless I need different case mappings.
    // Try both locally rather than changing the application's default locale.
    for (const bool turkishCasing : {false, true}) {
        result.intents.clear();
        const QStringList clauses = normalizeCommand(transcript, turkishCasing).split(separator, Qt::KeepEmptyParts);
        bool matched = true;
        for (const QString& clause : clauses) {
            if (clause.isEmpty() || !matchClause(clause.trimmed(), vehicleState, result.intents)) {
                matched = false;
                break;
            }
        }
        if (matched && !result.intents.isEmpty()) {
            result.success = true;
            return result;
        }
    }
    result.intents.clear();
    result.success = false;
    result.errorMessage = QStringLiteral("No recognized voice command");
    return result;
}
