"""Boids flocking: separation, alignment, cohesion, and goal seeking."""

from __future__ import annotations

import math
from dataclasses import dataclass

from geometry_msgs.msg import PoseStamped, TwistStamped
from nav_msgs.msg import Odometry


Vec3 = tuple[float, float, float]
Neighbor = tuple[Vec3, Vec3]  # position, velocity
ZERO: Vec3 = (0.0, 0.0, 0.0)


def _xyz(vector) -> Vec3:
    return (float(vector.x), float(vector.y), float(vector.z))


def _add(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _subtract(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _scale(vector: Vec3, factor: float) -> Vec3:
    return (vector[0] * factor, vector[1] * factor, vector[2] * factor)


def _length(vector: Vec3) -> float:
    return math.sqrt(vector[0] ** 2 + vector[1] ** 2 + vector[2] ** 2)


def _limit_speed(vector: Vec3, max_speed: float) -> Vec3:
    speed = _length(vector)
    if speed <= 1e-9 or speed <= max_speed:
        return vector
    return _scale(vector, max_speed / speed)


@dataclass(frozen=True)
class BoidsConfig:
    """Flocking distances, steering weights, and altitude control settings."""

    neighbor_radius: float = 6.0
    min_separation: float = 1.5

    separation_weight: float = 1.0
    alignment_weight: float = 0.3
    cohesion_weight: float = 0.6
    goal_weight: float = 0.5

    max_speed: float = 4.0

    use_3d: bool = False
    hold_altitude: float = 2.0
    altitude_gain: float = 1.0


class BoidsAlgorithm:
    def __init__(self, config: BoidsConfig) -> None:
        self._config = config

    def _nearby_neighbors(
        self, position: Vec3, neighbors: list[Odometry],
    ) -> list[Neighbor]:
        nearby: list[Neighbor] = []
        for odometry in neighbors:
            other_position = _xyz(odometry.pose.pose.position)
            distance = _length(_subtract(other_position, position))
            if distance < 1e-6 or distance > self._config.neighbor_radius:
                continue
            nearby.append((other_position, _xyz(odometry.twist.twist.linear)))
        return nearby

    def _flocking_vectors(
        self, position: Vec3, velocity: Vec3, neighbors: list[Neighbor],
    ) -> tuple[Vec3, Vec3, Vec3]:
        """Return separation, alignment, and cohesion steering vectors."""
        if not neighbors:
            return ZERO, ZERO, ZERO

        separation = ZERO
        position_sum = ZERO
        velocity_sum = ZERO
        for other_position, other_velocity in neighbors:
            away = _subtract(position, other_position)
            distance = max(1e-6, _length(away))
            strength = 1.0 / (distance * distance)
            if distance < self._config.min_separation:
                strength *= self._config.min_separation / distance
            separation = _add(separation, _scale(away, strength))
            position_sum = _add(position_sum, other_position)
            velocity_sum = _add(velocity_sum, other_velocity)

        inverse_count = 1.0 / len(neighbors)
        alignment = _subtract(_scale(velocity_sum, inverse_count), velocity)
        cohesion = _subtract(_scale(position_sum, inverse_count), position)
        return separation, alignment, cohesion

    def compute_setpoint(
        self,
        *,
        self_odom: Odometry,
        neighbors: list[Odometry],
        goal: PoseStamped | None,
        now_ns: int,
    ) -> TwistStamped:
        """Compute a velocity setpoint; the agent filters stale neighbors."""
        config = self._config
        position = _xyz(self_odom.pose.pose.position)
        velocity = _xyz(self_odom.twist.twist.linear)
        goal_position = _xyz(goal.pose.position) if goal is not None else position

        nearby = self._nearby_neighbors(position, neighbors)
        separation, alignment, cohesion = self._flocking_vectors(
            position, velocity, nearby,
        )
        goal_direction = _subtract(goal_position, position)

        steering = ZERO
        steering = _add(steering, _scale(separation, config.separation_weight))
        steering = _add(steering, _scale(alignment, config.alignment_weight))
        steering = _add(steering, _scale(cohesion, config.cohesion_weight))
        steering = _add(steering, _scale(goal_direction, config.goal_weight))

        if not config.use_3d:
            steering = (steering[0], steering[1], 0.0)
        command_velocity = _limit_speed(steering, config.max_speed)

        if not config.use_3d:
            target_altitude = (
                goal_position[2] if goal is not None else config.hold_altitude
            )
            altitude_error = target_altitude - position[2]
            vertical_speed = max(
                -config.max_speed,
                min(config.max_speed, config.altitude_gain * altitude_error),
            )
            command_velocity = (
                command_velocity[0], command_velocity[1], vertical_speed,
            )

        command = TwistStamped()
        command.header.stamp = self_odom.header.stamp
        command.header.frame_id = self_odom.header.frame_id or "map"
        command.twist.linear.x = float(command_velocity[0])
        command.twist.linear.y = float(command_velocity[1])
        command.twist.linear.z = float(command_velocity[2])
        return command
