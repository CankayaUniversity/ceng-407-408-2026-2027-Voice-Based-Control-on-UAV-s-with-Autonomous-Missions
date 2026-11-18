from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument("namespace", default_value="/uav_comms"),
            DeclareLaunchArgument("publish_hz", default_value="2.0"),
            Node(
                package="uav_comms_mission_planner",
                executable="mission_planner",
                name="mission_planner",
                namespace=LaunchConfiguration("namespace"),
                output="screen",
                parameters=[
                    {
                        "publish_hz": ParameterValue(
                            LaunchConfiguration("publish_hz"), value_type=float
                        ),
                    }
                ],
            ),
        ]
    )

