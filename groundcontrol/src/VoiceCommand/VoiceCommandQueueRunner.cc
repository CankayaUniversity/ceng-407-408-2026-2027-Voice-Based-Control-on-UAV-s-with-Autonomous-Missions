#include "VoiceCommandQueueRunner.h"

#include <utility>

#include "Commands/VoiceCommandFactory.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

VoiceCommandQueueRunner::VoiceCommandQueueRunner(QObject* parent) : QObject(parent)
{
    _builder = [](const MissionIntent& intent) { return VoiceCommandFactory::create(intent); };
    _pollTimer.setInterval(100);
    _postConditionTimer.setSingleShot(true);
    _postConditionTimer.setInterval(3000);
    (void) connect(&_pollTimer, &QTimer::timeout, this, &VoiceCommandQueueRunner::_evaluate);
    (void) connect(&_postConditionTimer, &QTimer::timeout, this, [this]() {
        const quint64 generation = _generation;
        _evaluate();
        if (_running && _generation == generation && !_postConditionSatisfied) {
            _fail(tr("%1 command is failed while executing").arg(_intents.at(_index).displaySummary));
        }
    });
}

void VoiceCommandQueueRunner::setCommandBuilder(CommandBuilder builder)
{
    if (!_running && builder) {
        _builder = std::move(builder);
    }
}

void VoiceCommandQueueRunner::start(const QList<MissionIntent>& intents, Vehicle* vehicle)
{
    if (_running) {
        return;
    }
    ++_generation;
    _intents = intents;
    _vehicle = vehicle;
    _index = 0;
    _running = true;
    if (!_vehicleAvailable()) {
        _fail(tr("Vehicle is not available"));
    } else if (_intents.isEmpty()) {
        _fail(tr("No commands to execute"));
    } else {
        _startNext();
    }
}

bool VoiceCommandQueueRunner::_vehicleAvailable() const
{
    MultiVehicleManager* manager = MultiVehicleManager::instance();
    return _vehicle && manager && manager->activeVehicle() == _vehicle;
}

void VoiceCommandQueueRunner::_startNext()
{
    if (!_running || !_vehicleAvailable()) {
        if (_running) {
            _fail(tr("Vehicle is not available"));
        }
        return;
    }
    if (_index >= _intents.size()) {
        _clear();
        emit sequenceFinished();
        return;
    }

    _command = _builder(_intents.at(_index));
    if (!_command) {
        _fail(tr("Unsupported voice command"));
        return;
    }
    const quint64 generation = _generation;
    emit stepStarted(_index);
    if (!_running || generation != _generation) {
        return;
    }
    if (!_vehicleAvailable()) {
        if (_running) {
            _fail(tr("Vehicle is not available"));
        }
        return;
    }
    // The previous step may have changed armed/flying/mode telemetry. Never
    // validate later commands against the state at approval time.
    if (!_command->checkPreCondition(_vehicle)) {
        _fail(tr("Pre-condition of the command is not satisfied"));
        return;
    }
    if (!_command->execute(_vehicle)) {
        _fail(tr("%1 command is failed while executing").arg(_intents.at(_index).displaySummary));
        return;
    }
    if (!_running) {
        return;
    }
    _postConditionSatisfied = false;
    _postConditionTimer.start();
    _pollTimer.start();
    _evaluate();
}

void VoiceCommandQueueRunner::_evaluate()
{
    if (!_running) {
        return;
    }
    if (!_vehicleAvailable()) {
        _fail(tr("Vehicle is not available"));
        return;
    }
    if (!_postConditionSatisfied) {
        if (!_command->checkPostCondition(_vehicle)) {
            return;
        }
        _postConditionSatisfied = true;
        _postConditionTimer.stop();
    }
    if (!_command->isCompleted(_vehicle)) {
        return;
    }

    _pollTimer.stop();
    _command.reset();
    ++_index;
    const quint64 generation = _generation;
    QTimer::singleShot(0, this, [this, generation]() {
        if (_running && generation == _generation) {
            _startNext();
        }
    });
}

void VoiceCommandQueueRunner::_clear()
{
    ++_generation;
    _running = false;
    _pollTimer.stop();
    _postConditionTimer.stop();
    _command.reset();
    _intents.clear();
    _vehicle.clear();
    _index = 0;
    _postConditionSatisfied = false;
}

void VoiceCommandQueueRunner::cancel()
{
    if (_running) {
        _clear();
    }
}

void VoiceCommandQueueRunner::_fail(const QString& message)
{
    const int index = _index;
    _clear();
    emit sequenceFailed(index, message);
}
