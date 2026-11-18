"""Deterministic checks for the local Boids velocity controller."""

import math
import unittest

from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Odometry
from uav_comms_swarm_agent.algorithms import BoidsAlgorithm, BoidsConfig


def odom(x=0.0, y=0.0, z=2.0, vx=0.0, vy=0.0, vz=0.0):
    result = Odometry()
    result.header.frame_id = "map"
    result.pose.pose.position.x = float(x)
    result.pose.pose.position.y = float(y)
    result.pose.pose.position.z = float(z)
    result.twist.twist.linear.x = float(vx)
    result.twist.twist.linear.y = float(vy)
    result.twist.twist.linear.z = float(vz)
    return result


def goal(x=0.0, y=0.0, z=2.0):
    result = PoseStamped()
    result.pose.position.x = float(x)
    result.pose.position.y = float(y)
    result.pose.position.z = float(z)
    return result


class BoidsTests(unittest.TestCase):
    def command(self, config=None, *, own=None, neighbors=None, target=None):
        return BoidsAlgorithm(config or BoidsConfig()).compute_setpoint(
            self_odom=own or odom(),
            neighbors=neighbors or [],
            goal=target,
            now_ns=0,
        )

    def test_goal_velocity_and_speed_limit(self):
        command = self.command(BoidsConfig(max_speed=2.0, goal_weight=1.0),
                               target=goal(3, 4))
        self.assertAlmostEqual(command.twist.linear.x, 1.2)
        self.assertAlmostEqual(command.twist.linear.y, 1.6)
        self.assertAlmostEqual(command.twist.linear.z, 0.0)

    def test_separation_increases_inside_minimum_distance(self):
        config = BoidsConfig(separation_weight=1.0, alignment_weight=0.0,
                             cohesion_weight=0.0, goal_weight=0.0)
        command = self.command(config, neighbors=[odom(x=1)])
        self.assertAlmostEqual(command.twist.linear.x, -1.5)

    def test_alignment_and_cohesion_use_neighbor_averages(self):
        neighbors = [odom(x=2, vx=3), odom(x=4, vx=5)]
        alignment = BoidsConfig(separation_weight=0.0, alignment_weight=1.0,
                                cohesion_weight=0.0, goal_weight=0.0)
        cohesion = BoidsConfig(separation_weight=0.0, alignment_weight=0.0,
                               cohesion_weight=1.0, goal_weight=0.0)
        self.assertAlmostEqual(
            self.command(alignment, own=odom(vx=1), neighbors=neighbors)
            .twist.linear.x, 3.0,
        )
        self.assertAlmostEqual(
            self.command(cohesion, neighbors=neighbors).twist.linear.x, 3.0,
        )

    def test_distant_and_coincident_neighbors_are_ignored(self):
        config = BoidsConfig(goal_weight=0.0)
        command = self.command(
            config, neighbors=[odom(x=7), odom(), odom(x=1e-7)],
        )
        self.assertEqual(command.twist.linear.x, 0.0)
        self.assertEqual(command.twist.linear.y, 0.0)

    def test_planar_altitude_follows_goal_or_hold_height(self):
        config = BoidsConfig(max_speed=2.0, goal_weight=1.0,
                             hold_altitude=4.0, altitude_gain=2.0)
        with_goal = self.command(config, target=goal(x=5, z=10))
        self.assertAlmostEqual(with_goal.twist.linear.x, 2.0)
        self.assertAlmostEqual(with_goal.twist.linear.z, 2.0)
        without_goal = self.command(config)
        self.assertAlmostEqual(without_goal.twist.linear.x, 0.0)
        self.assertAlmostEqual(without_goal.twist.linear.z, 2.0)
        self.assertGreater(math.hypot(with_goal.twist.linear.x,
                                      with_goal.twist.linear.z), config.max_speed)

    def test_3d_goal_clamps_total_speed(self):
        command = self.command(
            BoidsConfig(use_3d=True, goal_weight=1.0, max_speed=2.0),
            target=goal(x=3, z=6),
        )
        self.assertAlmostEqual(command.twist.linear.x, 1.2)
        self.assertAlmostEqual(command.twist.linear.z, 1.6)

    def test_header_is_copied_and_empty_frame_defaults_to_map(self):
        own = odom()
        own.header.stamp.sec = 42
        own.header.frame_id = ""
        command = self.command(own=own)
        self.assertEqual(command.header.stamp.sec, 42)
        self.assertEqual(command.header.frame_id, "map")
        self.assertEqual(command.twist.angular.z, 0.0)


if __name__ == "__main__":
    unittest.main()
