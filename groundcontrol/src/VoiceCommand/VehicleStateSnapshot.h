#pragma once

#include <QtCore/QString>

struct VehicleStateSnapshot
{
    bool isConnected = false;
    bool isArmed = false;
    bool isFlying = false;
    double currentAltitudeM = 0.0;
    QString flightMode;
    int vehicleId = -1;
};
