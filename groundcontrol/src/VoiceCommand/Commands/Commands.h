#pragma once

#include <QtCore/QPointer>

#include "VoiceCommandInterface.h"

namespace CommandHelpers {
bool positiveNumberInRange(const QVariant& value, double maxValue);
double positiveVehicleParam(Vehicle* vehicle, const QString& name);
double tightenLimit(double current, double candidate);
}  // namespace CommandHelpers

class Command : public VoiceCommandInterface
{
public:
    explicit Command(const MissionIntent& intent) : _intent(intent) {}

    virtual bool hasValidParameters() const { return true; }

protected:
    Vehicle* targetVehicle() const;
    bool available(Vehicle* vehicle) const;
    bool guided(Vehicle* vehicle) const;
    bool dispatchedTo(Vehicle* vehicle) const;
    void dispatched(Vehicle* vehicle);

    MissionIntent _intent;
    QPointer<Vehicle> _vehicle;
    bool _dispatched = false;
};
