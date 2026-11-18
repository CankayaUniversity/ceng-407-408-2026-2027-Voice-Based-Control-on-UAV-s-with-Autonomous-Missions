from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node


def _launch_setup(context, *args, **kwargs):
    vehicle_id = int(LaunchConfiguration("vehicle_id").perform(context))
    base_namespace = LaunchConfiguration("namespace")
    vehicle_namespace = f"px4_{vehicle_id}"
    full_namespace = PathJoinSubstitution([base_namespace, vehicle_namespace])

    return [
        Node(
            package="uav_comms_swarm_agent",
            executable="swarm_agent",
            name=f"swarm_agent_{vehicle_id}",
            namespace=full_namespace,
            output="screen",
            parameters=[
                {
                    "algorithm": LaunchConfiguration("algorithm"),
                    "swarm_namespace": base_namespace,
                }
            ],
            remappings=[
                ("swarms", PathJoinSubstitution([base_namespace, "swarms"])),
                ("swarm/members", PathJoinSubstitution([base_namespace, "swarm/members"])),
            ],
        )
    ]


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument("namespace", default_value="/uav_comms"),
            DeclareLaunchArgument("vehicle_id", default_value="0"),
            DeclareLaunchArgument("algorithm", default_value="boids"),
            OpaqueFunction(function=_launch_setup),
        ]
    )
