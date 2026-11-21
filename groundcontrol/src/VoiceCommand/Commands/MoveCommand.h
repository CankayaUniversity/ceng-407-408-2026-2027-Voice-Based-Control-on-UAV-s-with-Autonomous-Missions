#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtPositioning/QGeoCoordinate>

#include "Commands.h"

class MoveCommand final : public Command
{
public:
    explicit MoveCommand(const MissionIntent& intent);

    ~MoveCommand() override;

    bool checkPreCondition(Vehicle* vehicle) const override;
    bool execute(Vehicle* vehicle) override;
    bool checkPostCondition(Vehicle* vehicle) const override;

private:
    static QString offboardMode(Vehicle* vehicle);
    bool sendVelocity(double speed);
    void update();
    void stop();

    QTimer _timer;
    QElapsedTimer _elapsed;
    QGeoCoordinate _start;
    QString _mode;
    double _north = 0.0;
    double _east = 0.0;
    double _distance = 0.0;
    double _speed = 0.0;
    bool _modeRequested = false;
    bool _complete = false;
};
