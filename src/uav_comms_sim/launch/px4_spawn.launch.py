import os
import shlex

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.substitutions import EnvironmentVariable, LaunchConfiguration


PX4_GZ_NO_FOLLOW = "1"
DEFAULT_GZ_PARTITION = "default"
DEFAULT_GZ_IP = "127.0.0.1"


def _normalize_model_pose(pose: str) -> str:
    try:
        values = [p.strip() for p in str(pose).split(",") if p.strip() != ""]
    except ValueError as exc:
        raise ValueError(f"Invalid pose values: {pose!r}") from exc
    if len(values) == 2:
        x, y = values
        z, roll, pitch, yaw = 0.0, 0.0, 0.0, 0.0
    elif len(values) == 3:
        x, y, z = values
        roll, pitch, yaw = 0.0, 0.0, 0.0
    elif len(values) == 6:
        x, y, z, roll, pitch, yaw = values
    else:
        raise ValueError(
            f"model_pose must be 'x,y', 'x,y,z' or 'x,y,z,roll,pitch,yaw': {pose!r}"
        )
    return f"{x},{y},{z},{roll},{pitch},{yaw}"


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
    px4_sys_autostart = LaunchConfiguration("px4_sys_autostart").perform(context)
    px4_sim_model = LaunchConfiguration("px4_sim_model").perform(context)
    vehicle_id = LaunchConfiguration("vehicle_id").perform(context)
    model_pose = _normalize_model_pose(LaunchConfiguration("model_pose").perform(context))
    gz_partition = LaunchConfiguration("gz_partition").perform(context)
    gz_ip = LaunchConfiguration("gz_ip").perform(context)

    pkg_share_dir = get_package_share_directory("uav_comms_sim")
    pkg_worlds_dir = os.path.join(pkg_share_dir, "worlds")

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
    uxrce_namespace = "" if vehicle_id == "0" else f"px4_{vehicle_id}"

    px4_cmd = (
        f"{gz_shell_setup}"
        f" && env "
        f"PX4_UXRCE_DDS_NS={shlex.quote(uxrce_namespace)} "
        f"PX4_GZ_NO_FOLLOW={shlex.quote(PX4_GZ_NO_FOLLOW)} "
        f"PX4_GZ_WORLD={shlex.quote(gz_world)} "
        f"PX4_GZ_STANDALONE=1 "
        f"PX4_SYS_AUTOSTART={shlex.quote(px4_sys_autostart)} "
        f"PX4_SIM_MODEL={shlex.quote(px4_sim_model)} "
        f"PX4_GZ_MODEL_POSE={shlex.quote(model_pose)} "
        f"./build/px4_sitl_default/bin/px4 -i {shlex.quote(vehicle_id)}"
    )

    return [
        ExecuteProcess(
            cmd=["bash", "-c", px4_cmd],
            output="screen",
        )
    ]


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
                default_value="baylands",
                description="Gazebo world name (PX4_GZ_WORLD)",
            ),
            DeclareLaunchArgument(
                "px4_sys_autostart",
                default_value="4001",
                description="PX4 autostart ID (PX4_SYS_AUTOSTART)",
            ),
            DeclareLaunchArgument(
                "px4_sim_model",
                default_value="gz_x500",
                description="PX4 sim model name (PX4_SIM_MODEL)",
            ),
            DeclareLaunchArgument(
                "vehicle_id",
                default_value="0",
                description="PX4 instance ID (-i)",
            ),
            DeclareLaunchArgument(
                "model_pose",
                default_value="0,0",
                description="Pose string for PX4_GZ_MODEL_POSE: x,y or x,y,z or x,y,z,roll,pitch,yaw",
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
            OpaqueFunction(function=_launch_setup),
        ]
    )
