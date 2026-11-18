"""Deterministic local-controller checks; no flight simulator or mission planner."""
import math
import unittest

from geometry_msgs.msg import TwistStamped
from nav_msgs.msg import Odometry
from uav_comms_swarm_agent.collision_avoidance import CollisionAvoidance, CollisionAvoidanceConfig


def odom(x, y=0.0, z=3.0, vx=0.0, vy=0.0):
    result = Odometry()
    result.header.frame_id = 'map'
    result.pose.pose.position.x = float(x)
    result.pose.pose.position.y = float(y)
    result.pose.pose.position.z = float(z)
    result.twist.twist.linear.x = float(vx)
    result.twist.twist.linear.y = float(vy)
    return result


def desired(vx, vy=0.0, vz=0.0):
    result = TwistStamped()
    result.twist.linear.x = float(vx)
    result.twist.linear.y = float(vy)
    result.twist.linear.z = float(vz)
    return result


class CollisionAvoidanceTests(unittest.TestCase):
    def setUp(self):
        self.controller = CollisionAvoidance(CollisionAvoidanceConfig())

    def apply(self, target, own, other):
        return self.controller.apply(target, own, [('b', other)], own_topic='a')

    def test_imminent_collision_overrides_goal_velocity(self):
        target = desired(4, vz=0.5)
        result = self.apply(target, odom(0), odom(1.5))
        self.assertLess(result.twist.linear.x, 0)
        self.assertNotEqual(result.twist.linear.y, 0)
        self.assertLessEqual(math.hypot(result.twist.linear.x, result.twist.linear.y), 4.0)
        self.assertEqual(result.twist.linear.z, 0.5)
        self.assertEqual(target.twist.linear.x, 4.0)

    def test_nonconflicting_traffic_leaves_setpoint_unchanged(self):
        target = desired(2)
        obstacles = [odom(10), odom(3, z=8), odom(-3), odom(3, y=4)]
        for other in obstacles:
            with self.subTest(position=other.pose.pose.position):
                self.assertIs(self.apply(target, odom(0), other), target)

    def test_predicted_crossing_and_takeoff_conflict(self):
        target = desired(2)
        result = self.apply(target, odom(-3), odom(0, y=-3, vy=2))
        self.assertNotEqual((result.twist.linear.x, result.twist.linear.y), (2.0, 0.0))
        climb = desired(0, vz=1)
        result = self.apply(climb, odom(0, z=0), odom(0.5, z=2))
        self.assertNotEqual((result.twist.linear.x, result.twist.linear.y), (0.0, 0.0))
        self.assertEqual(result.twist.linear.z, 1.0)

    def test_coincident_positions_choose_opposite_finite_escape_directions(self):
        first = self.controller.apply(desired(0), odom(0), [('b', odom(0))], own_topic='a')
        second = self.controller.apply(desired(0), odom(0), [('a', odom(0))], own_topic='b')
        for value in (first.twist.linear.x, first.twist.linear.y,
                      second.twist.linear.x, second.twist.linear.y):
            self.assertTrue(math.isfinite(value))
        self.assertAlmostEqual(first.twist.linear.x, -second.twist.linear.x)
        self.assertAlmostEqual(first.twist.linear.y, -second.twist.linear.y)
        self.assertGreater(abs(first.twist.linear.x) + abs(first.twist.linear.y), 0)

    def test_invalid_or_different_frame_obstacle_is_ignored(self):
        target = desired(2)
        invalid = odom(float('nan'))
        different_frame = odom(1)
        different_frame.header.frame_id = 'other_map'
        self.assertIs(self.apply(target, odom(0), invalid), target)
        self.assertIs(self.apply(target, odom(0), different_frame), target)

    def test_bad_configuration_is_rejected(self):
        for kwargs in ({'horizon_s': 0}, {'radius': 1}, {'max_speed': float('inf')}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                CollisionAvoidanceConfig(**kwargs)

    def test_head_on_and_crossing_trajectories_keep_clearance(self):
        for start, goals in [
            ([odom(-5), odom(5)], [(5, 0), (-5, 0)]),
            ([odom(-5), odom(0, y=-5)], [(5, 0), (0, 5)]),
        ]:
            with self.subTest(goals=goals):
                vehicles = start
                min_distance = math.inf
                for _ in range(500):
                    targets = []
                    for vehicle, (gx, gy) in zip(vehicles, goals):
                        p = vehicle.pose.pose.position
                        dx, dy = gx - p.x, gy - p.y
                        scale = min(1.0, 2.0 / max(1e-6, math.hypot(dx, dy)))
                        targets.append(desired(dx * scale, dy * scale))
                    commands = [
                        self.controller.apply(targets[i], vehicles[i],
                                              [(str(1-i), vehicles[1-i])], own_topic=str(i))
                        for i in (0, 1)
                    ]
                    for vehicle, command in zip(vehicles, commands):
                        p, v = vehicle.pose.pose.position, command.twist.linear
                        p.x += v.x * 0.02
                        p.y += v.y * 0.02
                        vehicle.twist.twist.linear.x = v.x
                        vehicle.twist.twist.linear.y = v.y
                    a, b = (vehicle.pose.pose.position for vehicle in vehicles)
                    min_distance = min(min_distance, math.hypot(a.x-b.x, a.y-b.y))
                self.assertGreaterEqual(min_distance, 1.9)
                for vehicle, (gx, gy) in zip(vehicles, goals):
                    p = vehicle.pose.pose.position
                    self.assertLess(math.hypot(gx-p.x, gy-p.y), 0.5)
