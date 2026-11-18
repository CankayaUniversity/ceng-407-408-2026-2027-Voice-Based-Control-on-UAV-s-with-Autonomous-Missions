from __future__ import annotations

import math
from functools import partial

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from uav_comms_interfaces.srv import SetGoalPose
from uav_comms_interfaces.msg import SwarmGoal, Swarms


class MissionPlanner(Node):
    """
    Creates independent goal services and publishers for registered swarms.
    """

    def __init__(self, **node_kwargs) -> None:
        super().__init__("mission_planner", **node_kwargs)

        self.declare_parameter("publish_hz", 2.0)

        goal_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self._goal_qos = goal_qos
        self._swarm_goal_pubs = {}
        self._swarm_goal_state_pubs = {}
        self._swarm_goal_services = {}
        self._goals: dict[int, PoseStamped] = {}
        self.create_subscription(
            Swarms, "swarms", self._on_swarms, goal_qos
        )

        hz = float(self.get_parameter("publish_hz").value)
        self._timer = self.create_timer(1.0 / max(hz, 0.1), self._tick)

        self.get_logger().info(
            "MissionPlanner ready: create a swarm, then call swarm_<id>/set_goal."
        )

    def _on_swarms(self, msg: Swarms) -> None:
        for swarm in msg.swarms:
            swarm_id = swarm.swarm_id
            if swarm_id in self._swarm_goal_pubs:
                continue
            self._swarm_goal_pubs[swarm_id] = self.create_publisher(
                PoseStamped, f"swarm_{swarm_id}/goal", self._goal_qos
            )
            self._swarm_goal_state_pubs[swarm_id] = self.create_publisher(
                SwarmGoal, f"swarm_{swarm_id}/goal_state", self._goal_qos
            )
            self._publish_goal(swarm_id)
            self._swarm_goal_services[swarm_id] = self.create_service(
                SetGoalPose, f"swarm_{swarm_id}/set_goal",
                partial(self._on_set_goal, swarm_id=swarm_id),
            )

    def _on_set_goal(
        self, request: SetGoalPose.Request, response: SetGoalPose.Response,
        *, swarm_id: int,
    ):
        goal = request.goal
        if not all(math.isfinite(value) for value in (
            goal.pose.position.x, goal.pose.position.y, goal.pose.position.z
        )):
            response.message = "Goal coordinates must be finite."
            return response
        if not goal.header.frame_id:
            goal.header.frame_id = "map"

        self._goals[swarm_id] = goal
        self._publish_goal(swarm_id)

        response.accepted = True
        response.message = f"Goal updated for swarm {swarm_id}."
        return response

    def _tick(self) -> None:
        for swarm_id in self._swarm_goal_pubs:
            self._publish_goal(swarm_id)

    def _publish_goal(self, swarm_id: int) -> None:
        state = SwarmGoal()
        goal = self._goals.get(swarm_id)
        state.has_goal = goal is not None
        if goal is not None:
            msg = PoseStamped()
            msg.header.frame_id = goal.header.frame_id
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.pose = goal.pose
            self._swarm_goal_pubs[swarm_id].publish(msg)
            state.goal = msg
        self._swarm_goal_state_pubs[swarm_id].publish(state)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = MissionPlanner()
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
