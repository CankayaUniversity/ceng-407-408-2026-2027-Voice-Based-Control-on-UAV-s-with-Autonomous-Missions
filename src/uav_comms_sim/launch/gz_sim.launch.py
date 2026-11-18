import os
import shlex

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction, RegisterEventHandler, TimerAction
from launch.conditions import UnlessCondition
from launch.event_handlers import OnProcessStart
from launch.substitutions import EnvironmentVariable, LaunchConfiguration


DEFAULT_GZ_WORLD = "warehouse"
DEFAULT_GZ_VERBOSE = "1"
DEFAULT_GZ_PARTITION = "default"
DEFAULT_GZ_IP = "127.0.0.1"


def _gz_shell_setup(*, px4_dir: str, gz_env_sh: str, pkg_worlds_dir: str, gz_partition: str, gz_ip: str) -> str:
    return (
        f"cd {shlex.quote(px4_dir)}"
        f" && source {shlex.quote(gz_env_sh)}"
        f" && export GZ_PARTITION={shlex.quote(gz_partition)}"
        f" && export GZ_IP={shlex.quote(gz_ip)}"
        f" && export GZ_SIM_RESOURCE_PATH={shlex.quote(pkg_worlds_dir)}:$GZ_SIM_RESOURCE_PATH"
    )


def _launch_setup(context, *args, **kwargs):
    px4_dir = LaunchConfiguration("px4_dir").perform(context)
    gz_world = LaunchConfiguration("gz_world").perform(context)
    gz_verbose = LaunchConfiguration("gz_verbose").perform(context)
    gz_partition = LaunchConfiguration("gz_partition").perform(context)
    gz_ip = LaunchConfiguration("gz_ip").perform(context)

    pkg_share_dir = get_package_share_directory("uav_comms_sim")
    pkg_worlds_dir = os.path.join(pkg_share_dir, "worlds")
    gz_world_sdf = os.path.join(pkg_worlds_dir, f"{gz_world}.sdf")

    if not px4_dir:
        raise RuntimeError(
            "px4_dir is empty. Set env PX4_DIR or pass px4_dir:=/path/to/PX4-Autopilot"
        )

    gz_env_sh = os.path.join(px4_dir, "build", "px4_sitl_default", "rootfs", "gz_env.sh")
    if not os.path.exists(gz_env_sh):
        raise RuntimeError(f"PX4 gz env not found: {gz_env_sh}")

    gz_shell_setup = _gz_shell_setup(
        px4_dir=px4_dir,
        gz_env_sh=gz_env_sh,
        pkg_worlds_dir=pkg_worlds_dir,
        gz_partition=gz_partition,
        gz_ip=gz_ip,
    )

    gz_server_cmd = (
        f"{gz_shell_setup}"
        f" && gz sim --verbose={shlex.quote(gz_verbose)} -r -s {shlex.quote(gz_world_sdf)}"
    )
    gz_gui_cmd = f"{gz_shell_setup} && gz sim -g"

    gz_server = ExecuteProcess(
        cmd=["bash", "-c", gz_server_cmd],
        output="screen",
    )

    start_gui_after_server = RegisterEventHandler(
        OnProcessStart(
            target_action=gz_server,
            on_start=[
                TimerAction(
                    period=2.0,
                    actions=[
                        ExecuteProcess(
                            cmd=["bash", "-c", gz_gui_cmd],
                            output="screen",
                            condition=UnlessCondition(LaunchConfiguration("headless")),
                        )
                    ],
                )
            ],
        )
    )

    return [gz_server, start_gui_after_server]


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "namespace",
                default_value="/uav_comms",
                description="ROS namespace applied by parent launch files",
            ),
            DeclareLaunchArgument(
                "px4_dir",
                default_value=EnvironmentVariable("PX4_DIR", default_value=""),
                description="Path to PX4-Autopilot checkout (defaults to env PX4_DIR)",
            ),
            DeclareLaunchArgument(
                "gz_world",
                default_value=DEFAULT_GZ_WORLD,
                description="Gazebo world name and SDF basename (loaded from uav_comms_sim/worlds)",
            ),
            DeclareLaunchArgument(
                "gz_verbose",
                default_value=DEFAULT_GZ_VERBOSE,
                description="Gazebo verbosity level for `gz sim --verbose`",
            ),
            DeclareLaunchArgument(
                "gz_partition",
                default_value=DEFAULT_GZ_PARTITION,
                description="Gazebo partition name (GZ_PARTITION)",
            ),
            DeclareLaunchArgument(
                "gz_ip",
                default_value=DEFAULT_GZ_IP,
                description="Gazebo IP (GZ_IP)",
            ),
            DeclareLaunchArgument(
                "headless",
                default_value="false",
                description="If true: do not start gz GUI (server still runs)",
            ),
            OpaqueFunction(function=_launch_setup),
        ]
    )
