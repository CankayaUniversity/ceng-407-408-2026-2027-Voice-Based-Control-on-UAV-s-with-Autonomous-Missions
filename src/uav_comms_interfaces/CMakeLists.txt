cmake_minimum_required(VERSION 3.8)
project(uav_comms_interfaces)

find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/SwarmMember.msg"
  "msg/SwarmMembers.msg"
  "msg/Swarm.msg"
  "msg/Swarms.msg"
  "msg/SwarmGoal.msg"
  "srv/SetGoalPose.srv"
  "srv/CreateSwarm.srv"
  "srv/ConscriptDrone.srv"
  DEPENDENCIES builtin_interfaces geometry_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
