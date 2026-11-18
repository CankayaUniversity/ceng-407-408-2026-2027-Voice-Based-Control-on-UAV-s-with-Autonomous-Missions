"""Bounded ROS service calls shared by the host swarm scripts."""
from __future__ import annotations

import argparse
import math
import re

import rclpy
from uav_comms_interfaces.srv import ConscriptDrone, CreateSwarm, SetGoalPose


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("create-swarm", "conscript-drone", "go-mission"))
    parser.add_argument("parameters", nargs="*", metavar="key:value")
    args = parser.parse_args(argv)
    required = {
        "create-swarm": {"id"},
        "conscript-drone": {"id", "swarm_id"},
        "go-mission": {"x", "y", "z", "swarm_id"},
    }[args.command]
    values = {}
    for item in args.parameters:
        key, separator, value = item.partition(":")
        if not separator or key not in required or key in values:
            parser.error(f"unexpected or duplicate parameter: {item}")
        try:
            if key in ("id", "swarm_id"):
                if not re.fullmatch(r"[0-9]+", value) or int(value) > 2**32 - 1:
                    raise ValueError
                values[key] = int(value)
            else:
                values[key] = float(value)
                if not math.isfinite(values[key]):
                    raise ValueError
        except ValueError:
            parser.error(f"invalid value for {key}: {value}")
    missing = required - values.keys()
    if missing:
        parser.error("missing parameters: " + ", ".join(sorted(missing)))
    return args.command, values


def main(argv=None):
    command, values = parse_arguments(argv)
    if command == "create-swarm":
        service_type = CreateSwarm
        service_name = "swarm/create"
        request = CreateSwarm.Request()
        request.swarm_id = values["id"]
    elif command == "conscript-drone":
        service_type = ConscriptDrone
        service_name = "swarm/conscript"
        request = ConscriptDrone.Request()
        request.vehicle_id = values["id"]
        request.swarm_id = values["swarm_id"]
    else:
        service_type = SetGoalPose
        service_name = f"swarm_{values['swarm_id']}/set_goal"
        request = SetGoalPose.Request()
        request.goal.header.frame_id = "map"
        request.goal.pose.position.x = values["x"]
        request.goal.pose.position.y = values["y"]
        request.goal.pose.position.z = values["z"]
        request.goal.pose.orientation.w = 1.0

    rclpy.init()
    node = rclpy.create_node("swarm_command", namespace="/uav_comms")
    try:
        client = node.create_client(service_type, service_name)
        if not client.wait_for_service(timeout_sec=10.0):
            print(f"Service /uav_comms/{service_name} unavailable. Start the simulation and create the swarm first.")
            return 1
        future = client.call_async(request)
        rclpy.spin_until_future_complete(node, future, timeout_sec=10.0)
        if not future.done():
            print(f"Timed out waiting for /uav_comms/{service_name}.")
            return 1
        response = future.result()
        print(response.message)
        return 0 if response.accepted else 1
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
