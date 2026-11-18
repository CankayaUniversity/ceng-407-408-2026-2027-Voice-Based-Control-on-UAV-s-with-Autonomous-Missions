from __future__ import annotations

import re

import rclpy
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy

from uav_comms_interfaces.msg import Swarm, SwarmMember, SwarmMembers, Swarms
from uav_comms_interfaces.srv import ConscriptDrone, CreateSwarm


def _parse_px4_odom_topic(topic: str) -> tuple[int, str] | None:
    """
    Parse a fully-qualified odometry topic into (vehicle_id, vehicle_namespace).

    Expected shape: `/<any>/<prefix>/px4_<id>/odometry`
    """
    if not topic or not topic.startswith("/"):
        return None

    parts = [p for p in topic.split("/") if p]
    if len(parts) < 2:
        return None
    if parts[-1] != "odometry":
        return None

    match = re.fullmatch(r"px4_(\d+)", parts[-2])
    if not match:
        return None

    vehicle_id = int(match.group(1))
    vehicle_namespace = "/" + "/".join(parts[:-1])
    return (vehicle_id, vehicle_namespace)


class SwarmRegistry(Node):
    """Discovers vehicles and manages explicit, disjoint swarm membership."""

    def __init__(self, **node_kwargs) -> None:
        super().__init__("swarm_registry", **node_kwargs)

        self.declare_parameter("publish_period_s", 0.5)

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self._pub = self.create_publisher(SwarmMembers, "swarm/members", qos)

        self._swarms_pub = self.create_publisher(Swarms, "swarms", qos)
        self._member_pubs = {}
        self._assignments: dict[int, int] = {}
        self.create_service(CreateSwarm, "swarm/create", self._on_create_swarm)
        self.create_service(ConscriptDrone, "swarm/conscript", self._on_conscript)

        period_s = float(self.get_parameter("publish_period_s").value)
        self._timer = self.create_timer(max(period_s, 0.1), self._tick)

        self.get_logger().info("SwarmRegistry running: publishing swarm/members")

    def _on_create_swarm(self, request, response):
        swarm_id = request.swarm_id
        if swarm_id in self._member_pubs:
            response.message = f"Swarm {swarm_id} already exists."
            return response
        self._member_pubs[swarm_id] = self.create_publisher(
            SwarmMembers, f"swarm_{swarm_id}/members", self._pub.qos_profile
        )
        self._tick()
        response.accepted = True
        response.message = f"Created swarm {swarm_id}."
        return response

    def _on_conscript(self, request, response):
        vehicle_id, swarm_id = request.vehicle_id, request.swarm_id
        if swarm_id not in self._member_pubs:
            response.message = f"Swarm {swarm_id} does not exist; create it first."
            return response
        discovered_ids = self._discover_members().keys()
        if vehicle_id not in discovered_ids:
            response.message = f"Drone {vehicle_id} is not active."
            return response
        previous = self._assignments.get(vehicle_id)
        self._assignments[vehicle_id] = swarm_id
        self._tick()
        response.accepted = True
        if previous is not None and previous != swarm_id:
            response.message = f"Drone {vehicle_id} moved from swarm {previous} to swarm {swarm_id}."
        else:
            response.message = f"Drone {vehicle_id} conscripted into swarm {swarm_id}."
        return response

    def _discover_members(self) -> dict[int, tuple[str, str]]:
        discovered: dict[int, tuple[str, str]] = {}
        prefix = self.get_namespace().rstrip("/") + "/"
        for topic_name, _types in self.get_topic_names_and_types():
            # Subscriptions alone must not keep a stopped drone discoverable.
            if not topic_name.startswith(prefix) or self.count_publishers(topic_name) == 0:
                continue
            parsed = _parse_px4_odom_topic(topic_name)
            if parsed is None:
                continue
            vehicle_id, vehicle_namespace = parsed
            discovered.setdefault(vehicle_id, (topic_name, vehicle_namespace))
        return discovered

    def _tick(self) -> None:
        discovered = self._discover_members()
        vehicle_ids = sorted(discovered.keys())
        msg = SwarmMembers()
        msg.stamp = self.get_clock().now().to_msg()
        msg.members = []
        for vehicle_id in vehicle_ids:
            odom_topic, vehicle_namespace = discovered[vehicle_id]

            member = SwarmMember()
            member.vehicle_id = int(vehicle_id)
            member.ros_namespace = str(vehicle_namespace)
            member.odom_topic = str(odom_topic)
            msg.members.append(member)

        self._pub.publish(msg)
        snapshot = Swarms()
        snapshot.stamp = msg.stamp
        for swarm_id, publisher in sorted(self._member_pubs.items()):
            members = [
                member for member in msg.members
                if self._assignments.get(member.vehicle_id) == swarm_id
            ]
            swarm = Swarm()
            swarm.swarm_id = swarm_id
            swarm.members = members
            snapshot.swarms.append(swarm)
            membership = SwarmMembers()
            membership.stamp = msg.stamp
            membership.members = members
            publisher.publish(membership)
        self._swarms_pub.publish(snapshot)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = SwarmRegistry()
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
