#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

#include "VoiceCommandInterface.h"

class Command : public VoiceCommandInterface
{
public:
    explicit Command(const MissionIntent& intent) : _intent(intent) {}

    ~Command() override;

    bool isFailed(Vehicle* vehicle) const override;

protected:
    bool available(Vehicle* vehicle) const;
    bool guided(Vehicle* vehicle) const;
    bool dispatchedTo(Vehicle* vehicle) const;
    void dispatched(Vehicle* vehicle);
    void watchForAck(Vehicle* vehicle, MAV_CMD command);
    bool acknowledgedBy(Vehicle* vehicle) const;

    MissionIntent _intent;
    QPointer<Vehicle> _vehicle;
    bool _dispatched = false;

private:
    QMetaObject::Connection _ackConnection;
    bool _awaitingAck = false;
    bool _ackAccepted = false;
    bool _ackFailed = false;
};
