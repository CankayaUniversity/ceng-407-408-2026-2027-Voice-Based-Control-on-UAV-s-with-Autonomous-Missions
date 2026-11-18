from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument("namespace", default_value="/uav_comms"),
            DeclareLaunchArgument("publish_period_s", default_value="0.5"),
            Node(
                package="uav_comms_mission_planner",
                executable="swarm_registry",
                name="swarm_registry",
                namespace=LaunchConfiguration("namespace"),
                output="screen",
                parameters=[
                    {
                        "publish_period_s": ParameterValue(
                            LaunchConfiguration("publish_period_s"), value_type=float
                        ),
                    }
                ],
            ),
        ]
    )

