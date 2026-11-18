"""Local horizontal avoidance, independent of swarm goals and flocking."""
from __future__ import annotations

import math
from copy import deepcopy
from dataclasses import dataclass

from geometry_msgs.msg import TwistStamped
from nav_msgs.msg import Odometry


@dataclass(frozen=True)
class CollisionAvoidanceConfig:
    radius: float = 6.0
    min_distance: float = 2.0
    horizon_s: float = 2.0
    vertical_clearance: float = 1.5
    max_speed: float = 4.0

    def __post_init__(self):
        values = (self.radius, self.min_distance, self.horizon_s,
                  self.vertical_clearance, self.max_speed)
        if not all(math.isfinite(value) and value > 0 for value in values):
            raise ValueError("Collision avoidance parameters must be finite and positive")
        if self.radius <= self.min_distance:
            raise ValueError("Avoidance radius must exceed the minimum distance")


class CollisionAvoidance:
    def __init__(self, config: CollisionAvoidanceConfig):
        self.config = config

    def apply(
        self, desired: TwistStamped, self_odom: Odometry,
        obstacles: list[tuple[str, Odometry]], *, own_topic: str,
    ) -> TwistStamped:
        cfg = self.config
        p = self_odom.pose.pose.position
        v = desired.twist.linear
        if not all(math.isfinite(value) for value in (p.x, p.y, p.z, v.x, v.y, v.z)):
            return desired

        threats = []
        for topic, odom in obstacles:
            if topic == own_topic or (odom.header.frame_id or "map") != (self_odom.header.frame_id or "map"):
                continue
            q = odom.pose.pose.position
            w = odom.twist.twist.linear
            if not all(math.isfinite(value) for value in (q.x, q.y, q.z, w.x, w.y, w.z)):
                continue
            dx, dy, dz = p.x - q.x, p.y - q.y, p.z - q.z
            distance = math.hypot(dx, dy)
            if distance > cfg.radius:
                continue
            # Include takeoff conflicts, but leave vertically separated traffic alone.
            future_dz = dz + (v.z - w.z) * cfg.horizon_s
            vertical_gap = 0.0 if dz * future_dz <= 0 else min(abs(dz), abs(future_dz))
            if vertical_gap >= cfg.vertical_clearance:
                continue
            rvx, rvy = v.x - w.x, v.y - w.y
            speed_sq = rvx * rvx + rvy * rvy
            closing = dx * rvx + dy * rvy
            closest_t = min(cfg.horizon_s, max(0.0, -closing / speed_sq)) if speed_sq > 1e-9 else 0.0
            closest_distance = math.hypot(dx + rvx * closest_t, dy + rvy * closest_t)
            if distance >= cfg.min_distance and (closing >= 0 or closest_distance >= cfg.min_distance * 1.1):
                continue
            if distance > 1e-6:
                nx, ny = dx / distance, dy / distance
            else:
                # Opposite escape directions even at coincident horizontal positions.
                nx, ny = (1.0 if own_topic < topic else -1.0), 0.0
            bound = w.x * nx + w.y * ny + (cfg.min_distance - distance) / cfg.horizon_s
            urgency = max(0.0, 1.0 - closest_distance / (cfg.min_distance * 1.1))
            threats.append((distance, topic, nx, ny, bound, urgency))

        if not threats:
            return desired

        vx, vy = v.x, v.y
        # Circulate around each other rather than stopping nose to nose.
        for _, _, nx, ny, _, urgency in sorted(threats):
            vx -= ny * cfg.max_speed * urgency
            vy += nx * cfg.max_speed * urgency
        # Prioritize separation over the desired velocity, respecting the speed cap.
        for _ in range(4):
            for _, _, nx, ny, bound, _ in sorted(threats):
                correction = max(0.0, bound - (vx * nx + vy * ny))
                vx += correction * nx
                vy += correction * ny
            speed = math.hypot(vx, vy)
            if speed > cfg.max_speed:
                vx *= cfg.max_speed / speed
                vy *= cfg.max_speed / speed

        result = deepcopy(desired)
        result.twist.linear.x = vx
        result.twist.linear.y = vy
        return result
