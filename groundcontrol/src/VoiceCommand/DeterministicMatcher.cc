#include "DeterministicMatcher.h"

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

namespace {

using Action = VoiceCommandAction;

MissionIntent makeIntent(Action action, const QString& summary, int vehicleId, const QVariantMap& parameters = {})
{
    return {.action = action, .parameters = parameters, .targetVehicleId = vehicleId, .displaySummary = summary};
}

bool matchClause(const QString& clause, const VehicleStateSnapshot& state, QList<MissionIntent>& intents)
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
        intents.append(makeIntent(Action::Arm, QStringLiteral("Arm"), state.vehicleId));
    } else if (disarm.match(clause).hasMatch()) {
        intents.append(makeIntent(Action::Disarm, QStringLiteral("Disarm"), state.vehicleId));
    } else if (land.match(clause).hasMatch()) {
        intents.append(makeIntent(Action::Land, QStringLiteral("Land"), state.vehicleId));
    } else if (hold.match(clause).hasMatch()) {
        intents.append(makeIntent(Action::Hold, QStringLiteral("Hold position"), state.vehicleId));
    } else if (rtl.match(clause).hasMatch()) {
        intents.append(makeIntent(Action::ReturnToLaunch, QStringLiteral("Return to launch"), state.vehicleId));
    } else if (launch.match(clause).hasMatch()) {
        // "Launch" is a high-level goal; primitive "take off" never adds an implicit arm step.
        if (state.isFlying) {
            return false;
        }
        if (!state.isArmed) {
            intents.append(makeIntent(Action::Arm, QStringLiteral("Arm"), state.vehicleId));
        }
        intents.append(makeIntent(Action::Takeoff, QStringLiteral("Take off"), state.vehicleId));
    } else if (const auto m = takeoff.match(clause); m.hasMatch()) {
        QVariantMap parameters;
        QString summary = QStringLiteral("Take off");
        if (!m.captured(1).isEmpty()) {
            const double altitudeM = m.captured(1).toDouble();
            if (altitudeM <= 0.0) {
                return false;
            }
            parameters.insert(QStringLiteral("altitude"), altitudeM);
            summary += QStringLiteral(" to %1 m").arg(m.captured(1));
        }
        intents.append(makeIntent(Action::Takeoff, summary, state.vehicleId, parameters));
    } else if (const auto m = altitude.match(clause); m.hasMatch()) {
        const double altitudeM = m.captured(1).toDouble();
        if (altitudeM <= 0.0) {
            return false;
        }
        intents.append(makeIntent(Action::ChangeAltitude, QStringLiteral("Change altitude to %1 m").arg(m.captured(1)),
                                  state.vehicleId, {{QStringLiteral("altitude"), altitudeM}}));
    } else if (const auto m = heading.match(clause); m.hasMatch()) {
        const double degrees = (m.captured(1).isEmpty() ? m.captured(2) : m.captured(1)).toDouble();
        if (degrees > 360.0) {
            return false;
        }
        intents.append(makeIntent(Action::ChangeHeading, QStringLiteral("Change heading to %1°").arg(degrees),
                                  state.vehicleId, {{QStringLiteral("heading"), degrees}}));
    } else if (const auto m = move.match(clause); m.hasMatch()) {
        static const QHash<QString, QString> translatedDirections{
            {QStringLiteral("back"), QStringLiteral("backward")}, {QStringLiteral("ileri"), QStringLiteral("forward")},
            {QStringLiteral("geri"), QStringLiteral("backward")}, {QStringLiteral("sola"), QStringLiteral("left")},
            {QStringLiteral("sağa"), QStringLiteral("right")},    {QStringLiteral("kuzeye"), QStringLiteral("north")},
            {QStringLiteral("güneye"), QStringLiteral("south")},  {QStringLiteral("doğuya"), QStringLiteral("east")},
            {QStringLiteral("batıya"), QStringLiteral("west")},
        };
        const QString rawDirection = m.captured(1).isEmpty() ? m.captured(4) : m.captured(1);
        const QString direction = translatedDirections.value(rawDirection, rawDirection);
        const double distance = (m.captured(2).isEmpty() ? m.captured(5) : m.captured(2)).toDouble();
        if (distance <= 0.0 || (!m.captured(3).isEmpty() && m.captured(3).toDouble() <= 0.0)) {
            return false;
        }
        QVariantMap parameters{{QStringLiteral("direction"), direction}, {QStringLiteral("distance"), distance}};
        QString summary = QStringLiteral("Move %1 %2 m").arg(direction).arg(distance);
        if (!m.captured(3).isEmpty()) {
            parameters.insert(QStringLiteral("speed"), m.captured(3).toDouble());
            summary += QStringLiteral(" at %1 m/s").arg(m.captured(3));
        }
        intents.append(makeIntent(Action::Move, summary, state.vehicleId, parameters));
    } else {
        return false;
    }
    return true;
}

}  // namespace

IntentParseResult DeterministicMatcher::parse(const QString& transcript, const VehicleStateSnapshot& vehicleState)
{
    IntentParseResult result;
    result.rawTranscript = transcript;

    static const QRegularExpression separator(
        QStringLiteral(R"(\s*,?\s+(?:and\s+then|and|then|ve\s+sonra|ve|sonra)\s+)"));
    static const QRegularExpression trailingPunctuation(QStringLiteral(R"([.!?]+$)"));
    QString command = transcript.trimmed().toLower();
    command.remove(trailingPunctuation);
    const QStringList clauses = command.split(separator, Qt::KeepEmptyParts);
    if (clauses.isEmpty()) {
        result.errorMessage = QStringLiteral("No recognized voice command");
        return result;
    }

    for (const QString& clause : clauses) {
        if (clause.isEmpty() || !matchClause(clause.trimmed(), vehicleState, result.intents)) {
            result.intents.clear();
            result.errorMessage = QStringLiteral("No recognized voice command");
            return result;
        }
    }
    result.success = !result.intents.isEmpty();
    if (!result.success) {
        result.errorMessage = QStringLiteral("No action required");
    }
    return result;
}
