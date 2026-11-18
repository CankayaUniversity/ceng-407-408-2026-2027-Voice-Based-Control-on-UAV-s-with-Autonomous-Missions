import os
import shutil

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    GroupAction,
    IncludeLaunchDescription,
    OpaqueFunction,
)


from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import EnvironmentVariable, LaunchConfiguration


def _as_bool(value: str) -> bool:
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def _launch_swarm(context, *args, **kwargs):
    swarm_config = LaunchConfiguration("swarm_config").perform(context)
    mission_enabled = _as_bool(LaunchConfiguration("mission_enabled").perform(context))
    swarm_agent_enabled = _as_bool(
        LaunchConfiguration("swarm_agent_enabled").perform(context)
    )
    # world_start_delay_s = float(
    #     LaunchConfiguration("world_start_delay_s").perform(context)
    # )
    # spawn_delay_s = float(LaunchConfiguration("spawn_delay_s").perform(context))
    base_namespace = LaunchConfiguration("namespace")

    actions = []

    if mission_enabled:
        mission_share = get_package_share_directory("uav_comms_mission_planner")
        mission_planner_launch = os.path.join(
            mission_share, "launch", "mission_planner.launch.py"
        )
        actions.append(
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(mission_planner_launch),
                launch_arguments={
                    "namespace": base_namespace,
                    "publish_hz": LaunchConfiguration("goal_publish_hz"),
                }.items(),
            ),
        )

    with open(swarm_config, "r", encoding="utf-8") as file_handle:
        config = yaml.safe_load(file_handle) or {}
    swarm = config.get("swarm", [])

    if not isinstance(swarm, list) or not swarm:
        print(f"Invalid or empty swarm list in {swarm_config}\n")
        print("Running with empty swarm.")
        return actions

    sim_share = get_package_share_directory("uav_comms_sim")
    bridge_share = get_package_share_directory("uav_comms_px4_bridge")
    agent_share = get_package_share_directory("uav_comms_swarm_agent")

    px4_spawn_launch = os.path.join(sim_share, "launch", "px4_spawn.launch.py")
    px4_bridge_launch = os.path.join(bridge_share, "launch", "px4_bridge.launch.py")
    swarm_agent_launch = os.path.join(agent_share, "launch", "swarm_agent.launch.py")

    for item in swarm:
        vehicle_id = int(item["vehicle_id"])
        model_pose = item.get("pose", "0,0,0")

        group_actions = [
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(px4_spawn_launch),
                launch_arguments={
                    "px4_dir": LaunchConfiguration("px4_dir"),
                    "gz_world": LaunchConfiguration("gz_world"),
                    "px4_sim_model": LaunchConfiguration("px4_sim_model"),
                    "vehicle_id": str(vehicle_id),
                    "px4_sys_autostart": str(item["px4_sys_autostart"]),
                    "model_pose": model_pose,
                    "gz_partition": LaunchConfiguration("gz_partition"),
                    "gz_ip": LaunchConfiguration("gz_ip"),
                    "namespace": base_namespace,
                }.items(),
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(px4_bridge_launch),
                launch_arguments={
                    "vehicle_id": str(vehicle_id),
                    "rate_hz": LaunchConfiguration("rate_hz"),
                    "model_pose": model_pose,
                    "viz_enabled": LaunchConfiguration("viz_enabled"),
                    "namespace": base_namespace,
                }.items(),
            ),
        ]

        if swarm_agent_enabled:
            group_actions.append(
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(swarm_agent_launch),
                    launch_arguments={
                        "namespace": base_namespace,
                        "vehicle_id": str(vehicle_id),
                        "algorithm": "boids",
                    }.items(),
                )
            )

        actions.append(GroupAction(actions=group_actions))

    return actions


def generate_launch_description():
    bringup_share = get_package_share_directory("uav_comms_bringup")
    default_swarm_config = os.path.join(bringup_share, "config", "swarm_setup.yaml")

    sim_share = get_package_share_directory("uav_comms_sim")
    gz_sim_launch = os.path.join(sim_share, "launch", "gz_sim.launch.py")
    mission_share = get_package_share_directory("uav_comms_mission_planner")
    swarm_registry_launch = os.path.join(
        mission_share, "launch", "swarm_registry.launch.py"
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "swarm_config",
                default_value=default_swarm_config,
                description="Path to swarm setup YAML (parsed by uav_comms_bringup)",
            ),
            DeclareLaunchArgument("namespace", default_value="/uav_comms"),
            DeclareLaunchArgument("rate_hz", default_value="20.0"),
            DeclareLaunchArgument(
                "px4_dir",
                default_value=EnvironmentVariable("PX4_DIR", default_value=""),
                description="Path to PX4-Autopilot checkout (defaults to env PX4_DIR)",
            ),
            DeclareLaunchArgument("gz_world", default_value="warehouse"),
            DeclareLaunchArgument("gz_verbose", default_value="1"),
            DeclareLaunchArgument("gz_partition", default_value="default"),
            DeclareLaunchArgument("gz_ip", default_value="127.0.0.1"),
            DeclareLaunchArgument("px4_sim_model", default_value="gz_x500"),
            DeclareLaunchArgument(
                "world_start_delay_s",
                default_value="2.0",
                description="Delay before spawning drones and helper nodes (seconds)",
            ),
            DeclareLaunchArgument(
                "spawn_delay_s",
                default_value="0.0",
                description="Delay between each per-drone action (seconds)",
            ),
            DeclareLaunchArgument("headless", default_value="false"),
            DeclareLaunchArgument("micro_xrce_agent", default_value="true"),
            DeclareLaunchArgument("micro_xrce_port", default_value="8888"),
            DeclareLaunchArgument("viz_enabled", default_value="false"),
            DeclareLaunchArgument("mission_enabled", default_value="true"),
            DeclareLaunchArgument("swarm_agent_enabled", default_value="true"),
            DeclareLaunchArgument("goal_publish_hz", default_value="2.0"),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(swarm_registry_launch),
                launch_arguments={
                    "namespace": LaunchConfiguration("namespace"),
                }.items(),
            ),
            ExecuteProcess(
                cmd=[
                    "MicroXRCEAgent" if shutil.which("MicroXRCEAgent") else "micro-xrce-dds-agent",
                    "udp4",
                    "-p",
                    LaunchConfiguration("micro_xrce_port"),
                ],
                output="screen",
                condition=IfCondition(LaunchConfiguration("micro_xrce_agent")),
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(gz_sim_launch),
                launch_arguments={
                    "px4_dir": LaunchConfiguration("px4_dir"),
                    "gz_world": LaunchConfiguration("gz_world"),
                    "gz_verbose": LaunchConfiguration("gz_verbose"),
                    "gz_partition": LaunchConfiguration("gz_partition"),
                    "gz_ip": LaunchConfiguration("gz_ip"),
                    "headless": LaunchConfiguration("headless"),
                    "namespace": LaunchConfiguration("namespace"),
                }.items(),
            ),
            OpaqueFunction(function=_launch_swarm),
        ]
    )
