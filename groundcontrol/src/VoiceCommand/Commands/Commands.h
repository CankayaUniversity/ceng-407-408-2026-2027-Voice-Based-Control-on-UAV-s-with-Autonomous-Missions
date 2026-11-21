#pragma once

#include <QtCore/QPointer>

#include "VoiceCommandInterface.h"

class Command : public VoiceCommandInterface
{
public:
    explicit Command(const MissionIntent& intent) : _intent(intent) {}

protected:
    bool available(Vehicle* vehicle) const;
    bool guided(Vehicle* vehicle) const;
    bool dispatchedTo(Vehicle* vehicle) const;
    void dispatched(Vehicle* vehicle);

    MissionIntent _intent;
    QPointer<Vehicle> _vehicle;
    bool _dispatched = false;
};
