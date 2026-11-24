#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <functional>
#include <memory>

#include "Commands/VoiceCommandInterface.h"

class Vehicle;

/// Executes an approved sequence against one vehicle, one step at a time.
class VoiceCommandQueueRunner : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VoiceCommandQueueRunner)

public:
    using CommandBuilder = std::function<std::unique_ptr<VoiceCommandInterface>(const MissionIntent&)>;

    explicit VoiceCommandQueueRunner(QObject* parent = nullptr);

    bool isRunning() const { return _running; }

    /// The vehicle is pinned for the entire sequence; switching vehicles aborts it.
    void start(const QList<MissionIntent>& intents, Vehicle* vehicle);
    void cancel();
    /// Allows tests and future dispatchers to supply commands without changing queue logic.
    void setCommandBuilder(CommandBuilder builder);
    void setCompletionTimeout(int milliseconds);

signals:
    void stepStarted(int index);
    void sequenceFinished();
    void sequenceFailed(int index, const QString& message);

private:
    void _startNext();
    void _evaluate();
    void _fail(const QString& message);
    bool _vehicleAvailable() const;
    void _clear();

    QList<MissionIntent> _intents;
    QPointer<Vehicle> _vehicle;
    std::unique_ptr<VoiceCommandInterface> _command;
    CommandBuilder _builder;
    QTimer _pollTimer;
    QTimer _postConditionTimer;
    QTimer _completionTimer;
    int _index = 0;
    quint64 _generation = 0;
    bool _running = false;
    bool _postConditionSatisfied = false;
};
