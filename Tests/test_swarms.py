"""ROS integration tests without Gazebo/PX4: python3 -m unittest discover -s tests."""
import contextlib
import io
import math
import subprocess
import sys
import time
import unittest
import uuid
from unittest.mock import Mock

import rclpy
from geometry_msgs.msg import TwistStamped
from nav_msgs.msg import Odometry
from px4_msgs.msg import VehicleCommand, VehicleLocalPosition, VehicleStatus
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.parameter import Parameter
from rclpy.qos import QoSProfile, ReliabilityPolicy
from std_msgs.msg import Bool
from uav_comms_interfaces.msg import SwarmGoal
from uav_comms_interfaces.srv import ConscriptDrone, CreateSwarm, SetGoalPose
from uav_comms_mission_planner.mission_planner import MissionPlanner
from uav_comms_mission_planner.swarm_cli import parse_arguments
from uav_comms_mission_planner.swarm_registry import SwarmRegistry
from uav_comms_px4_bridge.px4_bridge import Px4Bridge
from uav_comms_swarm_agent.swarm_agent import AgentState, SwarmAgent


class SwarmTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()

    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()

    def setUp(self):
        self.base = '/test_' + uuid.uuid4().hex
        self.executor = SingleThreadedExecutor()
        self.nodes = []
        self.registry = self.add(SwarmRegistry(namespace=self.base))
        self.planner = self.add(MissionPlanner(namespace=self.base))
        self.probe = self.add(Node('probe', namespace=self.base))
        self.odom_pubs = {
            i: self.probe.create_publisher(Odometry, f'px4_{i}/odometry', 10)
            for i in range(3)
        }
        self.agents = {i: self.agent(i) for i in (0, 1)}
        self.until(lambda: len(self.registry._discover_members()) == 3)

    def add(self, node):
        self.nodes.append(node)
        self.executor.add_node(node)
        return node

    def agent(self, vehicle_id):
        return self.add(SwarmAgent(
            namespace=f'{self.base}/px4_{vehicle_id}',
            parameter_overrides=[Parameter('swarm_namespace', value=self.base)],
            cli_args=['--ros-args', '-r', f'swarms:={self.base}/swarms',
                      '-r', f'swarm/members:={self.base}/swarm/members'],
        ))

    def until(self, condition, timeout=8):
        deadline = time.monotonic() + timeout
        while not condition():
            if time.monotonic() > deadline:
                self.fail('Timed out waiting for ROS state')
            self.executor.spin_once(timeout_sec=0.05)

    def call(self, kind, name, **fields):
        client = self.probe.create_client(kind, name)
        try:
            self.assertTrue(client.wait_for_service(timeout_sec=5), name)
            request = kind.Request()
            for key, value in fields.items():
                setattr(request, key, value)
            future = client.call_async(request)
            self.until(future.done)
            return future.result()
        finally:
            self.probe.destroy_client(client)

    def create(self, swarm_id):
        return self.call(CreateSwarm, 'swarm/create', swarm_id=swarm_id)

    def conscript(self, vehicle_id, swarm_id):
        return self.call(ConscriptDrone, 'swarm/conscript',
                         vehicle_id=vehicle_id, swarm_id=swarm_id)

    def goal(self, swarm_id, x):
        request = SetGoalPose.Request()
        request.goal.pose.position.x = x
        request.goal.pose.position.z = 3.0
        request.goal.pose.orientation.w = 1.0
        return self.call(SetGoalPose, f'swarm_{swarm_id}/set_goal', goal=request.goal)

    def tearDown(self):
        for node in reversed(self.nodes):
            self.executor.remove_node(node)
            node.destroy_node()
        self.executor.shutdown()

    def test_isolation_and_late_join(self):
        self.assertTrue(self.create(1).accepted)
        self.assertTrue(self.create(2).accepted)
        self.assertTrue(self.conscript(0, 1).accepted)
        self.assertTrue(self.conscript(1, 2).accepted)
        self.assertTrue(self.conscript(2, 1).accepted)
        self.assertTrue(self.goal(1, 12.0).accepted)
        self.until(lambda: self.agents[0]._goal is not None)
        self.assertIsNone(self.agents[1]._goal)
        self.until(lambda: self.agents[1]._state == AgentState.LANDING)
        self.assertEqual(self.agents[0]._current_neighbor_topics,
                         [f'{self.base}/px4_2/odometry'])
        self.assertTrue(self.goal(2, -5.0).accepted)
        self.until(lambda: self.agents[1]._goal is not None)
        self.assertEqual(self.agents[0]._goal.pose.position.x, 12.0)
        self.assertEqual(self.agents[1]._goal.pose.position.x, -5.0)
        late_agent = self.agent(2)
        self.until(lambda: late_agent._goal is not None)
        self.assertEqual(late_agent._swarm_id, 1)
        self.assertEqual(late_agent._goal.pose.position.x, 12.0)

    def test_validation_and_unassigned_drone(self):
        self.assertTrue(self.create(0).accepted)
        self.assertFalse(self.create(0).accepted)
        self.assertFalse(self.conscript(0, 99).accepted)
        self.assertFalse(self.conscript(99, 0).accepted)
        self.assertTrue(self.create(1).accepted)
        self.assertTrue(self.conscript(0, 0).accepted)
        self.assertTrue(self.conscript(0, 0).accepted)
        self.assertFalse(self.conscript(0, 99).accepted)
        self.assertEqual(self.registry._assignments[0], 0)
        self.assertTrue(self.goal(0, 9.0).accepted)
        self.until(lambda: self.agents[0]._goal is not None)
        self.assertIsNone(self.agents[1]._goal)
        self.assertIsNone(self.agents[1]._swarm_id)
        self.assertFalse(self.goal(0, float('nan')).accepted)
        self.assertEqual(self.planner._goals[0].pose.position.x, 9.0)

    def test_transfer_to_swarm_with_goal(self):
        self.assertTrue(self.create(1).accepted)
        self.assertTrue(self.create(2).accepted)
        self.assertTrue(self.conscript(0, 1).accepted)
        self.assertTrue(self.conscript(2, 1).accepted)
        self.assertTrue(self.conscript(1, 2).accepted)
        self.assertTrue(self.goal(1, 9.0).accepted)
        self.assertTrue(self.goal(2, -4.0).accepted)
        agent = self.agents[0]
        self.until(lambda: agent._goal is not None)
        odom = Odometry()
        odom.pose.pose.position.z = 2.0
        self.odom_pubs[0].publish(odom)
        self.until(lambda: agent._state == AgentState.MISSION)
        self.assertTrue(self.conscript(0, 2).accepted)
        self.until(lambda: agent._swarm_id == 2 and agent._goal is not None)
        self.until(lambda: self.agents[1]._current_neighbor_topics ==
                   [f'{self.base}/px4_0/odometry'])
        self.assertEqual(agent._goal.pose.position.x, -4.0)
        self.assertEqual(agent._state, AgentState.MISSION)
        self.assertEqual(agent._current_neighbor_topics, [f'{self.base}/px4_1/odometry'])
        self.assertEqual(self.registry._assignments, {0: 2, 1: 2, 2: 1})
        old_goal = SwarmGoal(has_goal=True)
        old_goal.goal.pose.position.x = 100.0
        agent._on_swarm_goal(old_goal, swarm_id=1)
        self.assertEqual(agent._goal.pose.position.x, -4.0)
        self.assertTrue(self.conscript(0, 2).accepted)
        self.assertEqual(agent._goal.pose.position.x, -4.0)

    def test_foreign_swarm_avoidance_and_stale_data(self):
        self.assertTrue(self.create(1).accepted)
        self.assertTrue(self.create(2).accepted)
        self.assertTrue(self.conscript(0, 1).accepted)
        self.assertTrue(self.conscript(1, 2).accepted)
        self.assertTrue(self.conscript(2, 1).accepted)
        agent = self.agents[0]
        foreign_topic = f'{self.base}/px4_1/odometry'
        peer_topic = f'{self.base}/px4_2/odometry'
        self.until(lambda: foreign_topic in agent._neighbor_subs and
                   agent._current_neighbor_topics == [peer_topic])
        own = Odometry()
        own.pose.pose.position.z = 3.0
        foreign = Odometry()
        foreign.pose.pose.position.x = 1.5
        foreign.pose.pose.position.z = 3.0
        peer = Odometry()
        peer.pose.pose.position.x = -10.0
        peer.pose.pose.position.z = 3.0
        self.odom_pubs[0].publish(own)
        self.odom_pubs[1].publish(foreign)
        self.odom_pubs[2].publish(peer)
        self.until(lambda: agent._self_odom is not None and
                   len(agent._neighbors_by_topic) == 2)
        desired = TwistStamped()
        desired.twist.linear.x = 4.0
        now = agent.get_clock().now().nanoseconds
        self.assertEqual(agent._neighbors(now), [agent._neighbors_by_topic[peer_topic][0]])
        avoided = agent._avoid_collisions(desired, now)
        self.assertLess(avoided.twist.linear.x, 0.0)
        self.assertNotEqual(avoided.twist.linear.y, 0.0)
        self.assertEqual(desired.twist.linear.x, 4.0)
        self.assertIs(agent._avoid_collisions(desired, now + 1_000_000_000), desired)
        agent.set_parameters([Parameter('collision_avoidance_enabled', value=False)])
        self.assertIs(agent._avoid_collisions(desired, now), desired)
        agent.set_parameters([Parameter('collision_avoidance_enabled', value=True)])

        # Moving into the same swarm changes its role to a flocking peer.
        self.assertTrue(self.conscript(1, 1).accepted)
        self.until(lambda: foreign_topic in agent._current_neighbor_topics)
        self.assertIs(agent._avoid_collisions(desired, agent.get_clock().now().nanoseconds), desired)
        self.probe.destroy_publisher(self.odom_pubs.pop(1))
        self.until(lambda: foreign_topic not in agent._neighbor_subs)
        self.assertNotIn(foreign_topic, agent._neighbors_by_topic)

    def test_avoidance_applies_to_takeoff_and_mission_but_not_landing(self):
        agent = self.agents[0]
        agent._timer.cancel()
        agent._self_odom = Odometry()
        agent._self_odom.pose.pose.position.z = 1.0
        agent._goal = SetGoalPose.Request().goal
        agent._goal.pose.position.x = 10.0
        agent._pub_setpoint.publish = Mock()
        agent._avoid_collisions = Mock(side_effect=lambda sp, now: sp)
        for state in (AgentState.TAKEOFF, AgentState.MISSION):
            agent._state = state
            agent._tick()
        self.assertEqual(agent._avoid_collisions.call_count, 2)
        self.assertEqual(agent._pub_setpoint.publish.call_count, 2)
        agent._state = AgentState.LANDING
        agent._tick()
        self.assertEqual(agent._avoid_collisions.call_count, 2)
        self.assertEqual(agent._pub_setpoint.publish.call_count, 2)

    def test_transfer_without_goal_lands_and_new_goal_resumes(self):
        bridge = self.add(Px4Bridge(namespace=f'{self.base}/px4_0'))
        commands = []
        qos = QoSProfile(depth=100, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.probe.create_subscription(
            VehicleCommand, f'{self.base}/px4_0/fmu/in/vehicle_command',
            commands.append, qos,
        )
        status = VehicleStatus()
        status.system_id = 1
        status.arming_state = VehicleStatus.ARMING_STATE_ARMED
        status.nav_state = VehicleStatus.NAVIGATION_STATE_OFFBOARD
        status.pre_flight_checks_pass = True
        bridge._on_status(status)
        position = VehicleLocalPosition(xy_valid=True, z_valid=True)
        bridge._on_local_position(position)
        self.assertTrue(self.create(1).accepted)
        self.assertTrue(self.create(2).accepted)
        self.assertTrue(self.conscript(0, 1).accepted)
        self.assertTrue(self.goal(1, 9.0).accepted)
        odom = Odometry()
        odom.pose.pose.position.z = 2.0
        self.odom_pubs[0].publish(odom)
        self.until(lambda: bridge._target_twist is not None)

        self.assertTrue(self.conscript(0, 2).accepted)
        self.until(lambda: bridge._landing_requested and any(
            cmd.command == VehicleCommand.VEHICLE_CMD_NAV_LAND for cmd in commands
        ))
        agent = self.agents[0]
        self.assertEqual(agent._state, AgentState.LANDING)
        self.assertIsNone(agent._goal)
        self.assertIsNone(bridge._target_twist)
        self.assertEqual(bridge._setpoint_counter, 0)
        self.assertEqual(agent._current_neighbor_topics, [])

        # PX4 completes the landing and disarms. A new goal must release the
        # landing latch and allow fresh takeoff setpoints and arming again.
        status.arming_state = VehicleStatus.ARMING_STATE_DISARMED
        status.nav_state = VehicleStatus.NAVIGATION_STATE_AUTO_LAND
        bridge._on_status(status)
        self.assertTrue(self.goal(2, -3.0).accepted)
        self.until(lambda: not bridge._landing_requested and bridge._target_twist is not None)
        self.assertEqual(agent._goal.pose.position.x, -3.0)
        self.until(lambda: any(
            cmd.command == VehicleCommand.VEHICLE_CMD_COMPONENT_ARM_DISARM
            and cmd.param1 == 1.0 for cmd in commands
        ))

    def test_bridge_landing_overrides_cached_twists_and_arming(self):
        bridge = self.add(Px4Bridge(namespace=f'{self.base}/landing_test'))
        self.assertEqual(
            bridge.status_sub.topic_name,
            f'{self.base}/landing_test/fmu/out/vehicle_status_v1',
        )
        bridge.timer.cancel()
        bridge._timestamp_us = Mock(return_value=10_000_000)
        bridge._publish_vehicle_command = Mock()
        bridge._publish_offboard_setpoints = Mock()
        status = VehicleStatus(
            system_id=1, arming_state=VehicleStatus.ARMING_STATE_ARMED,
            nav_state=VehicleStatus.NAVIGATION_STATE_OFFBOARD,
        )
        bridge._on_status(status)
        old_twist = TwistStamped()
        old_twist.twist.linear.x = 4.0
        bridge._on_target_twist(old_twist)
        bridge._on_land_request(Bool(data=True))
        bridge._tick()
        command, = bridge._publish_vehicle_command.call_args.args
        self.assertEqual(command, VehicleCommand.VEHICLE_CMD_NAV_LAND)
        self.assertTrue(all(math.isnan(v) for v in bridge._publish_vehicle_command.call_args.kwargs.values()))
        bridge._on_target_twist(old_twist)
        bridge._on_land_request(Bool(data=True))
        bridge._tick()
        self.assertIsNone(bridge._target_twist)
        self.assertEqual(bridge._publish_vehicle_command.call_count, 1)
        bridge._timestamp_us.return_value += 1_100_000
        bridge._tick()
        self.assertEqual(bridge._publish_vehicle_command.call_count, 2)
        status.nav_state = VehicleStatus.NAVIGATION_STATE_AUTO_LAND
        bridge._timestamp_us.return_value += 1_100_000
        bridge._tick()
        status.arming_state = VehicleStatus.ARMING_STATE_DISARMED
        bridge._tick()
        self.assertEqual(bridge._publish_vehicle_command.call_count, 2)
        bridge._publish_offboard_setpoints.assert_not_called()
        bridge._on_land_request(Bool(data=False))
        bridge._tick()
        bridge._publish_offboard_setpoints.assert_not_called()
        self.assertIsNone(bridge._target_twist)
        self.assertEqual(bridge._setpoint_counter, 0)

    def test_stopped_drone_and_rejoin(self):
        self.assertTrue(self.create(1).accepted)
        self.assertTrue(self.conscript(0, 1).accepted)
        self.assertTrue(self.goal(1, 7.0).accepted)
        self.until(lambda: self.agents[0]._goal is not None)
        self.probe.destroy_publisher(self.odom_pubs.pop(0))
        self.until(lambda: self.agents[0]._swarm_id is None)
        self.assertIsNone(self.agents[0]._goal)
        self.assertFalse(self.conscript(0, 1).accepted)
        self.odom_pubs[0] = self.probe.create_publisher(Odometry, 'px4_0/odometry', 10)
        self.until(lambda: self.agents[0]._goal is not None)
        self.assertEqual(self.agents[0]._goal.pose.position.x, 7.0)

    def test_command_service_calls_and_exit_codes(self):
        self.add(SwarmRegistry(namespace='/uav_comms'))
        planner = self.add(MissionPlanner(namespace='/uav_comms'))
        self.probe.create_publisher(Odometry, '/uav_comms/px4_8/odometry', 10)

        def command(*args):
            process = subprocess.Popen(
                [sys.executable, '-m', 'uav_comms_mission_planner.swarm_cli', *args],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            try:
                self.until(lambda: process.poll() is not None, timeout=25)
                stdout, stderr = process.communicate()
                return process.returncode, stdout + stderr
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()

        self.assertEqual(command('create-swarm', 'id:17')[0], 0)
        self.assertEqual(command('create-swarm', 'id:17')[0], 1)
        self.assertEqual(command('conscript-drone', 'id:8')[0], 2)
        self.assertEqual(command('conscript-drone', 'id:8', 'swarm_id:17')[0], 0)
        self.assertEqual(command('conscript-drone', 'id:999', 'swarm_id:17')[0], 1)
        self.assertEqual(command('go-mission', 'x:1', 'y:2', 'z:3', 'swarm_id:17')[0], 0)
        self.assertEqual(planner._goals[17].pose.position.z, 3.0)


class ArgumentTests(unittest.TestCase):
    def test_required_ids_and_coordinates(self):
        command, values = parse_arguments(['go-mission', 'swarm_id:1', 'z:3', 'x:1', 'y:2'])
        self.assertEqual(command, 'go-mission')
        self.assertEqual(values, dict(x=1.0, y=2.0, z=3.0, swarm_id=1))
        self.assertEqual(parse_arguments(['create-swarm', 'id:0'])[1], {'id': 0})
        cases = [
            ['conscript-drone', 'id:1'],
            ['create-swarm', 'id:-1'],
            ['create-swarm', 'id:4294967296'],
            ['create-swarm', 'id:1', 'id:2'],
            ['create-swarm', 'other:1'],
            ['go-mission', '1', '2', '3'],
            ['go-mission', 'x:nan', 'y:2', 'z:3', 'swarm_id:1'],
            ['go-mission', 'x:1e999', 'y:2', 'z:3', 'swarm_id:1'],
            ['create-swarm', 'id:$(touch /tmp/unwanted)'],
        ]
        for argv in cases:
            with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    parse_arguments(argv)
                self.assertEqual(error.exception.code, 2)
