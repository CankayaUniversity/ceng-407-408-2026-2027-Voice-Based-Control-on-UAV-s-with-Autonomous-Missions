from __future__ import annotations

from enum import Enum
from functools import partial

import rclpy
from geometry_msgs.msg import PoseStamped
from geometry_msgs.msg import TwistStamped
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import Bool

from uav_comms_interfaces.msg import SwarmGoal, SwarmMembers, Swarms

from .algorithms import BoidsAlgorithm, BoidsConfig
from .collision_avoidance import CollisionAvoidance, CollisionAvoidanceConfig


class AgentState(str, Enum):
    WAIT_GOAL = "WAIT_GOAL"
    TAKEOFF = "TAKEOFF"
    MISSION = "MISSION"
    LANDING = "LANDING"


class SwarmAgent(Node):
    """
    High-level swarm agent (per drone):
    - Waits for conscription and that swarm's goal before doing anything.
    - Performs a simple vertical takeoff to `takeoff_alt` using velocity setpoints.
    - After takeoff, runs the selected swarm algorithm (currently: Boids).

    Publishes: `target/twist`, `target/land` (in the vehicle namespace)
    Subscribes: `odometry`, fleet `swarms`, and the assigned swarm's goal state.
    """

    def __init__(self, **node_kwargs) -> None:
        super().__init__("swarm_agent", **node_kwargs)

        self.declare_parameter("rate_hz", 20.0)
        self.declare_parameter("algorithm", "boids")
        self.declare_parameter("swarm_namespace", "/uav_comms")

        self.declare_parameter("takeoff_alt", 2.0)
        self.declare_parameter("takeoff_reached_tol", 0.1)
        self.declare_parameter("takeoff_hold_s", 1.0)

        # Boids parameters (forwarded to algorithm config)
        self.declare_parameter("r_neighbor", 6.0)
        self.declare_parameter("neighbor_timeout_s", 0.5)
        self.declare_parameter("min_sep", 1.5)
        self.declare_parameter("k_sep", 2.0)
        self.declare_parameter("k_align", 0.6)
        self.declare_parameter("k_coh", 0.4)
        self.declare_parameter("k_goal", 0.8)
        self.declare_parameter("v_max", 4.0)
        self.declare_parameter("use_3d", False)
        self.declare_parameter("k_alt", 1.0)
        self.declare_parameter("collision_avoidance_enabled", True)
        self.declare_parameter("avoidance_radius", 6.0)
        self.declare_parameter("avoidance_min_distance", 2.0)
        self.declare_parameter("avoidance_horizon_s", 2.0)
        self.declare_parameter("avoidance_vertical_clearance", 1.5)

        self._rate_hz = float(self.get_parameter("rate_hz").value)

        goal_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        members_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )

        self._self_odom: Odometry | None = None
        self._goal: PoseStamped | None = None
        self._swarm_id: int | None = None
        self._goal_sub = None
        self._goal_qos = goal_qos
        self._hold_for_goal_state = False
        self._resume_mission_after_transfer = False
        self._neighbors_by_topic: dict[str, tuple[Odometry, int]] = {}
        self._neighbor_subs = {}
        self._current_neighbor_topics: list[str] = []
        self._fleet_odom_topics: set[str] = set()

        self._state: AgentState = AgentState.WAIT_GOAL
        self._takeoff_reached_s: float = 0.0

        self.create_subscription(Odometry, "odometry", self._on_self_odom, 10)
        self.create_subscription(
            Swarms, "swarms", self._on_swarms, members_qos
        )
        self.create_subscription(
            SwarmMembers, "swarm/members", self._on_fleet_members, members_qos
        )

        self._pub_setpoint = self.create_publisher(TwistStamped, "target/twist", 10)
        self._pub_land = self.create_publisher(Bool, "target/land", 10)

        self._algorithm = self._build_algorithm()
        self._collision_avoidance = CollisionAvoidance(CollisionAvoidanceConfig(
            radius=float(self.get_parameter("avoidance_radius").value),
            min_distance=float(self.get_parameter("avoidance_min_distance").value),
            horizon_s=float(self.get_parameter("avoidance_horizon_s").value),
            vertical_clearance=float(self.get_parameter("avoidance_vertical_clearance").value),
            max_speed=float(self.get_parameter("v_max").value),
        ))

        self._timer = self.create_timer(1.0 / max(self._rate_hz, 1e-6), self._tick)
        self.get_logger().info(
            "SwarmAgent ready: waits for conscription and a swarm goal."
        )

    def _own_odom_topic(self) -> str:
        ns = (self.get_namespace() or "/").rstrip("/")
        return f"{ns}/odometry" if ns else "/odometry"

    def _build_algorithm(self):
        algo = str(self.get_parameter("algorithm").value).strip().lower()
        if algo != "boids":
            raise RuntimeError(f"Unsupported algorithm: {algo!r}")

        takeoff_alt = float(self.get_parameter("takeoff_alt").value)
        cfg = BoidsConfig(
            neighbor_radius=float(self.get_parameter("r_neighbor").value),
            min_separation=float(self.get_parameter("min_sep").value),
            separation_weight=float(self.get_parameter("k_sep").value),
            alignment_weight=float(self.get_parameter("k_align").value),
            cohesion_weight=float(self.get_parameter("k_coh").value),
            goal_weight=float(self.get_parameter("k_goal").value),
            max_speed=float(self.get_parameter("v_max").value),
            use_3d=bool(self.get_parameter("use_3d").value),
            hold_altitude=takeoff_alt,
            altitude_gain=float(self.get_parameter("k_alt").value),
        )
        return BoidsAlgorithm(cfg)

    def _set_neighbor_topics(self, topics: list[str]) -> None:
        self._current_neighbor_topics = sorted({t for t in topics if t})
        self._sync_odom_subscriptions()

    def _on_fleet_members(self, msg: SwarmMembers) -> None:
        own_topic = self._own_odom_topic()
        self._fleet_odom_topics = {
            str(member.odom_topic) for member in msg.members
            if member.odom_topic and member.odom_topic != own_topic
        }
        self._sync_odom_subscriptions()

    def _sync_odom_subscriptions(self) -> None:
        topics = self._fleet_odom_topics | set(self._current_neighbor_topics)
        for topic in set(self._neighbor_subs) - topics:
            self.destroy_subscription(self._neighbor_subs.pop(topic))
            self._neighbors_by_topic.pop(topic, None)
        for topic in sorted(topics - self._neighbor_subs.keys()):
            self._neighbor_subs[topic] = self.create_subscription(
                Odometry, topic, partial(self._on_neighbor_odom, topic=topic), 10,
            )

    def _on_self_odom(self, msg: Odometry) -> None:
        self._self_odom = msg

    def _on_goal(self, msg: PoseStamped) -> None:
        self._goal = msg
        self._hold_for_goal_state = False
        self._pub_land.publish(Bool(data=False))
        if self._state in (AgentState.WAIT_GOAL, AgentState.LANDING):
            self._state = (
                AgentState.MISSION if self._resume_mission_after_transfer
                else AgentState.TAKEOFF
            )
            self._takeoff_reached_s = 0.0
        self._resume_mission_after_transfer = False

    def _on_swarms(self, msg: Swarms) -> None:
        own_odom = self._own_odom_topic()
        assigned = next((
            swarm for swarm in msg.swarms
            if any(member.odom_topic == own_odom for member in swarm.members)
        ), None)
        swarm_id = assigned.swarm_id if assigned is not None else None
        if swarm_id != self._swarm_id:
            self._resume_mission_after_transfer = (
                self._state == AgentState.MISSION or self._resume_mission_after_transfer
            )
            self._hold_for_goal_state = self._goal is not None
            if self._goal_sub is not None:
                self.destroy_subscription(self._goal_sub)
                self._goal_sub = None
            self._swarm_id = swarm_id
            self._goal = None
            if self._state != AgentState.LANDING:
                self._state = AgentState.WAIT_GOAL
            self._takeoff_reached_s = 0.0
            # Stop the previous mission while the retained goal state arrives.
            if self._hold_for_goal_state:
                self._publish_hold()
            if swarm_id is not None:
                base = str(self.get_parameter("swarm_namespace").value).rstrip("/")
                self._goal_sub = self.create_subscription(
                    SwarmGoal, f"{base}/swarm_{swarm_id}/goal_state",
                    partial(self._on_swarm_goal, swarm_id=swarm_id), self._goal_qos,
                )
            elif self._hold_for_goal_state:
                self._request_land()
        self._set_neighbor_topics([
            str(member.odom_topic) for member in assigned.members
            if member.odom_topic and member.odom_topic != own_odom
        ] if assigned is not None else [])

    def _on_swarm_goal(self, msg: SwarmGoal, *, swarm_id: int) -> None:
        # Discard any queued delivery from a previous subscription.
        if swarm_id == self._swarm_id:
            if msg.has_goal:
                self._on_goal(msg.goal)
            else:
                self._request_land()

    def _request_land(self) -> None:
        self._goal = None
        self._hold_for_goal_state = False
        self._resume_mission_after_transfer = False
        self._state = AgentState.LANDING
        self._pub_land.publish(Bool(data=True))

    def _publish_hold(self) -> None:
        sp = TwistStamped()
        sp.header.stamp = self.get_clock().now().to_msg()
        sp.header.frame_id = "map"
        self._pub_setpoint.publish(sp)

    def _on_neighbor_odom(self, msg: Odometry, topic: str) -> None:
        if topic in self._neighbor_subs:
            self._neighbors_by_topic[topic] = (msg, self.get_clock().now().nanoseconds)

    def _neighbors(self, now_ns: int) -> list[Odometry]:
        timeout_ns = int(
            float(self.get_parameter("neighbor_timeout_s").value) * 1_000_000_000
        )
        return [
            od
            for topic, (od, stamp_ns) in self._neighbors_by_topic.items()
            if topic in self._current_neighbor_topics and 0 <= now_ns - stamp_ns <= timeout_ns
        ]

    def _avoid_collisions(self, setpoint: TwistStamped, now_ns: int) -> TwistStamped:
        if not bool(self.get_parameter("collision_avoidance_enabled").value) or self._self_odom is None:
            return setpoint
        timeout_ns = int(float(self.get_parameter("neighbor_timeout_s").value) * 1_000_000_000)
        obstacles = [
            (topic, odom) for topic, (odom, stamp_ns) in self._neighbors_by_topic.items()
            if topic in self._fleet_odom_topics and topic not in self._current_neighbor_topics
            and 0 <= now_ns - stamp_ns <= timeout_ns
        ]
        return self._collision_avoidance.apply(
            setpoint, self._self_odom, obstacles, own_topic=self._own_odom_topic(),
        )

    def _tick(self) -> None:
        if self._state == AgentState.LANDING:
            self._pub_land.publish(Bool(data=True))
            return
        if self._goal is None:
            if self._hold_for_goal_state:
                self._publish_hold()
            return
        self._pub_land.publish(Bool(data=False))
        if self._self_odom is None:
            return

        dt_s = 1.0 / max(self._rate_hz, 1e-6)
        now_ns = self.get_clock().now().nanoseconds

        if self._state == AgentState.TAKEOFF:
            takeoff_alt = float(self.get_parameter("takeoff_alt").value)
            tol = float(self.get_parameter("takeoff_reached_tol").value)
            hold_s = float(self.get_parameter("takeoff_hold_s").value)
            v_max = float(self.get_parameter("v_max").value)
            k_alt = float(self.get_parameter("k_alt").value)

            z = float(self._self_odom.pose.pose.position.z)
            z_err = takeoff_alt - z
            vz = max(-v_max, min(v_max, k_alt * z_err))
            if abs(z_err) <= tol:
                vz = 0.0

            sp = TwistStamped()
            sp.header.stamp = self.get_clock().now().to_msg()
            sp.header.frame_id = "map"
            sp.twist.linear.x = 0.0
            sp.twist.linear.y = 0.0
            sp.twist.linear.z = float(vz)
            sp.twist.angular.x = 0.0
            sp.twist.angular.y = 0.0
            sp.twist.angular.z = 0.0
            self._pub_setpoint.publish(self._avoid_collisions(sp, now_ns))

            if abs(z_err) <= tol:
                self._takeoff_reached_s += dt_s
            else:
                self._takeoff_reached_s = 0.0

            if self._takeoff_reached_s >= hold_s:
                self._state = AgentState.MISSION
            return

        if self._state == AgentState.MISSION:
            neighbors = self._neighbors(now_ns)
            setpoint = self._algorithm.compute_setpoint(
                self_odom=self._self_odom,
                neighbors=neighbors,
                goal=self._goal,
                now_ns=now_ns,
            )
            self._pub_setpoint.publish(self._avoid_collisions(setpoint, now_ns))


def main(args=None) -> None:
    rclpy.init(args=args)
    node = SwarmAgent()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        print(f"Node {node.get_name()} interrupted")
    except Exception as e:
        print(f"Node {node.get_name()} shut down error: {e}")
    finally:
        print(f"Node {node.get_name()} shut down.")


if __name__ == "__main__":
    main()
