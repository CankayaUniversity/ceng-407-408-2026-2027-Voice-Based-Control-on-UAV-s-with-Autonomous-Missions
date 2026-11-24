#include "MoveCommand.h"

#include <QtCore/QtMath>
#include <algorithm>
#include <cmath>

#include "Fact.h"
#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

namespace {

double moveSpeed(const MissionIntent& intent, Vehicle* vehicle)
{
    if (intent.parameters.contains(QStringLiteral("speed"))) {
        return intent.parameters.value(QStringLiteral("speed")).toDouble();
    }

    const double cruise = vehicle->defaultCruiseSpeed();
    const double limit = vehicle->maximumHorizontalSpeedMultirotorMetersSecond();

    // The cruise setting is intended for fixed-wing mission planning; cap it for
    // voice-controlled multirotor Offboard movement.
    const double safeLimit = std::isfinite(limit) && limit > 0.0 ? std::min(5.0, limit) : 5.0;

    return std::min(std::isfinite(cruise) && cruise > 0.0 ? cruise : 1.0, safeLimit);
}

}  // namespace

MoveCommand::MoveCommand(const MissionIntent& intent) : Command(intent) {}

MoveCommand::~MoveCommand()
{
    stop();
}

bool MoveCommand::checkPreCondition(Vehicle* vehicle) const
{
    if (!guided(vehicle) || !vehicle->multiRotor() || !vehicle->armed() || !vehicle->flying() ||
        !vehicle->coordinate().isValid() || !vehicle->heading()->rawValue().isValid()) {
        return false;
    }

    const double speed = moveSpeed(_intent, vehicle);
    const double vehicleLimit = vehicle->maximumHorizontalSpeedMultirotorMetersSecond();

    return !offboardMode(vehicle).isEmpty() && std::isfinite(vehicle->heading()->rawValue().toDouble()) &&
           speed <= 5.0 && (!std::isfinite(vehicleLimit) || speed <= vehicleLimit);
}

bool MoveCommand::execute(Vehicle* vehicle)
{
    if (_dispatched || !checkPreCondition(vehicle)) {
        return false;
    }

    const QString direction = _intent.parameters.value(QStringLiteral("direction")).toString();
    double azimuth = 0.0;

    if (direction == QStringLiteral("north")) {
        azimuth = 0.0;
    } else if (direction == QStringLiteral("east")) {
        azimuth = 90.0;
    } else if (direction == QStringLiteral("south")) {
        azimuth = 180.0;
    } else if (direction == QStringLiteral("west")) {
        azimuth = 270.0;
    } else {
        azimuth = vehicle->heading()->rawValue().toDouble();
        if (direction == QStringLiteral("right")) {
            azimuth += 90.0;
        } else if (direction == QStringLiteral("backward")) {
            azimuth += 180.0;
        } else if (direction == QStringLiteral("left")) {
            azimuth -= 90.0;
        }
    }

    _north = std::cos(qDegreesToRadians(azimuth));
    _east = std::sin(qDegreesToRadians(azimuth));
    _start = vehicle->coordinate();
    _distance = _intent.parameters.value(QStringLiteral("distance")).toDouble();
    _speed = moveSpeed(_intent, vehicle);
    _mode = offboardMode(vehicle);

    dispatched(vehicle);

    QObject::connect(&_timer, &QTimer::timeout, &_timer, [this]() { update(); });
    _timer.setInterval(100);  // PX4 requires >2 Hz setpoints before and during Offboard.
    _elapsed.start();

    if (!sendVelocity(_speed)) {
        stop();
        return false;
    }

    _timer.start();

    return true;
}

bool MoveCommand::checkPostCondition(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && (_complete || (_modeRequested && vehicle->flightMode() == _mode));
}

bool MoveCommand::isCompleted(Vehicle* vehicle) const
{
    return dispatchedTo(vehicle) && _complete;
}

bool MoveCommand::isFailed(Vehicle* vehicle) const
{
    return vehicle && _dispatched && _vehicle == vehicle && _failed;
}

QString MoveCommand::offboardMode(Vehicle* vehicle)
{
    for (const QString& mode : vehicle->flightModes()) {
        if (mode.compare(QStringLiteral("Offboard"), Qt::CaseInsensitive) == 0) {
            return mode;
        }
    }

    return {};
}

bool MoveCommand::sendVelocity(double speed)
{
    if (!_vehicle || !available(_vehicle)) {
        return false;
    }

    const auto link = _vehicle->vehicleLinkManager()->primaryLink().lock();

    if (!link) {
        return false;
    }

    mavlink_message_t message{};

    // Ignore position, acceleration and yaw; velocity is in earth-fixed NED.
    constexpr uint16_t velocityOnlyMask = 0x0DC7;

    mavlink_msg_set_position_target_local_ned_pack_chan(
        MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId(), link->mavlinkChannel(), &message,
        0, _vehicle->id(), _vehicle->defaultComponentId(), MAV_FRAME_LOCAL_NED, velocityOnlyMask, 0, 0, 0,
        static_cast<float>(speed * _north), static_cast<float>(speed * _east), 0, 0, 0, 0, 0, 0);

    return _vehicle->sendMessageOnLinkThreadSafe(link.get(), message);
}

void MoveCommand::update()
{
    // Independent safety limit in case the caller fails to release a timed-out command.
    if (!_vehicle || !available(_vehicle) || !_vehicle->armed() || !_vehicle->flying() || _elapsed.elapsed() >= 30000) {
        fail();
        return;
    }

    const QGeoCoordinate position = _vehicle->coordinate();
    if (!position.isValid()) {
        fail();
        return;
    }

    const double traveled = _start.distanceTo(position);
    const double bearing = qDegreesToRadians(_start.azimuthTo(position));
    const double alongTrack = traveled * (std::cos(bearing) * _north + std::sin(bearing) * _east);

    if (alongTrack >= _distance && _modeRequested && _vehicle->flightMode() == _mode) {
        _complete = true;
        stop();
        return;
    }

    if (!sendVelocity(_speed)) {
        fail();
        return;
    }

    // Prime the Offboard stream for at least a second before switching modes.
    if (!_modeRequested && _elapsed.elapsed() >= 1100) {
        _modeRequested = true;
        _vehicle->setFlightMode(_mode);
    }

    if (_modeRequested && _elapsed.elapsed() >= 2500 && _vehicle->flightMode() != _mode) {
        fail();
    }
}

void MoveCommand::fail()
{
    _failed = true;
    stop();
}

void MoveCommand::stop()
{
    _timer.stop();

    if (_vehicle && _modeRequested && _vehicle->flightMode() == _mode) {
        (void) sendVelocity(0.0);
        _vehicle->pauseVehicle();
    }

    _modeRequested = false;
}
