import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import EnvironmentVariable, LaunchConfiguration


def _launch_setup(context, *args, **kwargs):
    px4_dir = LaunchConfiguration("px4_dir").perform(context)
    gz_world = LaunchConfiguration("gz_world").perform(context)
    gz_partition = LaunchConfiguration("gz_partition").perform(context)
    gz_ip = LaunchConfiguration("gz_ip").perform(context)
    px4_sim_model = LaunchConfiguration("px4_sim_model").perform(context)
    px4_sys_autostart = LaunchConfiguration("px4_sys_autostart").perform(context)
    vehicle_id = int(LaunchConfiguration("vehicle_id").perform(context))
    rate_hz = LaunchConfiguration("rate_hz").perform(context)
    viz_enabled = LaunchConfiguration("viz_enabled").perform(context)

    model_pose = LaunchConfiguration("model_pose").perform(context)

    sim_share = get_package_share_directory("uav_comms_sim")
    bridge_share = get_package_share_directory("uav_comms_px4_bridge")
    agent_share = get_package_share_directory("uav_comms_swarm_agent")

    px4_spawn_launch = os.path.join(sim_share, "launch", "px4_spawn.launch.py")
    px4_bridge_launch = os.path.join(bridge_share, "launch", "px4_bridge.launch.py")
    swarm_agent_launch = os.path.join(agent_share, "launch", "swarm_agent.launch.py")

    base_namespace = LaunchConfiguration("namespace")

    actions = [
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(px4_spawn_launch),
            launch_arguments={
                "px4_dir": px4_dir,
                "gz_world": gz_world,
                "px4_sim_model": px4_sim_model,
                "vehicle_id": str(vehicle_id),
                "px4_sys_autostart": px4_sys_autostart,
                "model_pose": model_pose,
                "gz_partition": gz_partition,
                "gz_ip": gz_ip,
                "namespace": base_namespace,
            }.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(px4_bridge_launch),
            launch_arguments={
                "vehicle_id": str(vehicle_id),
                "rate_hz": rate_hz,
                "model_pose": model_pose,
                "viz_enabled": viz_enabled,
                "namespace": base_namespace,
            }.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(swarm_agent_launch),
            condition=IfCondition(LaunchConfiguration("swarm_agent_enabled")),
            launch_arguments={
                "namespace": base_namespace,
                "vehicle_id": str(vehicle_id),
                "algorithm": "boids",
            }.items(),
        ),
    ]

    return actions


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument("namespace", default_value="/uav_comms"),
            DeclareLaunchArgument(
                "px4_dir",
                default_value=EnvironmentVariable("PX4_DIR", default_value=""),
                description="Path to PX4-Autopilot checkout (defaults to env PX4_DIR)",
            ),
            DeclareLaunchArgument("gz_world", default_value="warehouse"),
            DeclareLaunchArgument("gz_partition", default_value="default"),
            DeclareLaunchArgument("gz_ip", default_value="127.0.0.1"),
            DeclareLaunchArgument("px4_sim_model", default_value="gz_x500"),
            DeclareLaunchArgument("px4_sys_autostart", default_value="4001"),
            DeclareLaunchArgument("vehicle_id", default_value="0"),
            DeclareLaunchArgument(
                "model_pose",
                default_value="0,0",
                description="Pose string: x,y or x,y,z or x,y,z,roll,pitch,yaw",
            ),
            DeclareLaunchArgument("rate_hz", default_value="20.0"),
            DeclareLaunchArgument("viz_enabled", default_value="false"),
            DeclareLaunchArgument("swarm_agent_enabled", default_value="true"),
            OpaqueFunction(function=_launch_setup),
        ]
    )
