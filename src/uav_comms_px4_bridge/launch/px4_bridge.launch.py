from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def _launch_setup(context, *args, **kwargs):
    vehicle_id = int(LaunchConfiguration("vehicle_id").perform(context))
    vehicle_namespace = f"px4_{vehicle_id}"
    base_namespace = LaunchConfiguration("namespace")
    full_namespace = PathJoinSubstitution([base_namespace, vehicle_namespace])

    remappings = []
    fmu_root = "/fmu" if vehicle_id == 0 else f"/px4_{vehicle_id}/fmu"
    remappings = [
        ("fmu/in/offboard_control_mode", f"{fmu_root}/in/offboard_control_mode"),
        ("fmu/in/trajectory_setpoint", f"{fmu_root}/in/trajectory_setpoint"),
        ("fmu/in/vehicle_command", f"{fmu_root}/in/vehicle_command"),
        ("fmu/out/vehicle_status_v1", f"{fmu_root}/out/vehicle_status_v1"),
        (
            "fmu/out/vehicle_local_position_v1",
            f"{fmu_root}/out/vehicle_local_position_v1",
        ),
        ("fmu/out/vehicle_command_ack", f"{fmu_root}/out/vehicle_command_ack"),
        ("fmu/out/vehicle_odometry", f"{fmu_root}/out/vehicle_odometry"),
    ]

    return [
        Node(
            package="uav_comms_px4_bridge",
            executable="px4_bridge",
            name="px4_bridge",
            namespace=full_namespace,
            output="screen",
            remappings=remappings,
            parameters=[
                {
                    "rate_hz": LaunchConfiguration("rate_hz"),
                    "model_pose": LaunchConfiguration("model_pose"),
                    "viz_enabled": ParameterValue(
                        LaunchConfiguration("viz_enabled"), value_type=bool
                    ),
                }
            ],
        )
    ]


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument("namespace", default_value="/uav_comms"),
            DeclareLaunchArgument("vehicle_id", default_value="0"),
            DeclareLaunchArgument("rate_hz", default_value="20.0"),
            DeclareLaunchArgument(
                "model_pose",
                default_value="0,0",
                description="Vehicle ENU spawn pose: x,y or x,y,z (also accepts x,y,z,roll,pitch,yaw)",
            ),
            DeclareLaunchArgument("viz_enabled", default_value="false"),
            OpaqueFunction(function=_launch_setup),
        ]
    )
