import math
from dataclasses import dataclass

import rclpy
from geometry_msgs.msg import PoseStamped, TwistStamped
from nav_msgs.msg import Odometry, Path
from px4_msgs.msg import (
    OffboardControlMode,
    TrajectorySetpoint,
    VehicleCommand,
    VehicleCommandAck,
    VehicleLocalPosition,
    VehicleOdometry,
    VehicleStatus,
)
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from std_msgs.msg import Bool

from .frame_transforms import (
    Quaternion,
    transform_orientation_ned_to_enu,
    transform_vector_enu_to_ned,
    transform_vector_ned_to_enu,
)


@dataclass(frozen=True)
class NedVelocitySetpoint:
    vx: float
    vy: float
    vz: float
    yawspeed: float


class Px4Bridge(Node):
    def __init__(self, **node_kwargs) -> None:
        super().__init__("px4_bridge", **node_kwargs)

        self.declare_parameter("rate_hz", 20.0)
        self.declare_parameter("model_pose", "0,0,0")
        self.declare_parameter("viz_enabled", False)

        self.rate_hz = float(self.get_parameter("rate_hz").value)
        self.spawn_x_enu, self.spawn_y_enu, self.spawn_z_enu = self._parse_model_pose(
            self.get_parameter("model_pose").value
        )
        viz_value = self.get_parameter("viz_enabled").value
        if isinstance(viz_value, str):
            self.viz_enabled = viz_value.strip().lower() in ("1", "true", "yes", "on")
        else:
            self.viz_enabled = bool(viz_value)

        qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)

        self.offboard_mode_pub = self.create_publisher(
            OffboardControlMode,
            "fmu/in/offboard_control_mode",
            qos,
        )
        self.traj_sp_pub = self.create_publisher(
            TrajectorySetpoint,
            "fmu/in/trajectory_setpoint",
            qos,
        )
        self.vehicle_cmd_pub = self.create_publisher(
            VehicleCommand, "fmu/in/vehicle_command", qos
        )

        self.status_sub = self.create_subscription(
            VehicleStatus,
            "fmu/out/vehicle_status_v1",
            self._on_status,
            qos,
        )
        self.local_pos_sub = self.create_subscription(
            VehicleLocalPosition,
            "fmu/out/vehicle_local_position_v1",
            self._on_local_position,
            qos,
        )
        self.cmd_ack_sub = self.create_subscription(
            VehicleCommandAck,
            "fmu/out/vehicle_command_ack",
            self._on_vehicle_command_ack,
            qos,
        )

        self.vehicle_odom_sub = self.create_subscription(
            VehicleOdometry,
            "fmu/out/vehicle_odometry",
            self._on_vehicle_odometry,
            qos,
        )
        self.odom_pub = self.create_publisher(Odometry, "odometry", 10)
        self.path_pub = self.create_publisher(Path, "odometry_path", 10)

        self._vehicle_status: VehicleStatus | None = None
        self._local_pos: VehicleLocalPosition | None = None
        self._last_cmd_ack: VehicleCommandAck | None = None
        self._target_twist: TwistStamped | None = None
        self._last_vehicle_odom: VehicleOdometry | None = None
        self._path_msg: Path | None = None
        self._setpoint_counter = 0
        self._last_offboard_request_us = 0
        self._last_arm_request_us = 0
        self._last_preflight_warn_us = 0
        self._landing_requested = False
        self._last_land_request_us = None

        period_s = 1.0 / max(self.rate_hz, 1.0)
        self.timer = self.create_timer(period_s, self._tick)

        self.target_twist_sub = self.create_subscription(
            TwistStamped,
            "target/twist",
            self._on_target_twist,
            10,
        )
        self.land_sub = self.create_subscription(
            Bool, "target/land", self._on_land_request, 10
        )
        self.get_logger().info(
            f"spawn_offset_enu=({self.spawn_x_enu:.2f},{self.spawn_y_enu:.2f},{self.spawn_z_enu:.2f}) "
            f"from model_pose={self.get_parameter('model_pose').value!r}"
        )
        self.get_logger().info(f"viz_enabled={self.viz_enabled}")
        self.get_logger().info(f"{self.get_name()} initialized")

    def _parse_model_pose(self, value) -> tuple[float, float, float]:
        parts = [p.strip() for p in str(value).split(",") if p.strip() != ""]
        try:
            floats = [float(p) for p in parts]
        except ValueError as exc:
            raise ValueError(f"Invalid model_pose values: {value!r}") from exc

        if len(floats) == 2:
            x, y = floats
            z = 0.0
        elif len(floats) >= 3:
            x, y, z = floats[0], floats[1], floats[2]
        else:
            raise ValueError(
                f"model_pose must be 'x,y' or 'x,y,z' or 'x,y,z,roll,pitch,yaw': {value!r}"
            )
        return float(x), float(y), float(z)

    def _timestamp_us(self) -> int:
        return int(self.get_clock().now().nanoseconds // 1000)

    def _on_status(self, msg: VehicleStatus) -> None:
        self._vehicle_status = msg

    def _on_local_position(self, msg: VehicleLocalPosition) -> None:
        self._local_pos = msg

    def _on_target_twist(self, msg: TwistStamped) -> None:
        if not self._landing_requested:
            self._target_twist = msg

    def _on_land_request(self, msg: Bool) -> None:
        if msg.data == self._landing_requested:
            return
        self._landing_requested = msg.data
        # Never resume a cached velocity or re-arm while PX4 is landing.
        self._target_twist = None
        self._setpoint_counter = 0
        self._last_land_request_us = None
        self._last_offboard_request_us = 0
        self._last_arm_request_us = 0

    def _on_vehicle_odometry(self, msg: VehicleOdometry) -> None:
        self._last_vehicle_odom = msg
        self._publish_ros_odometry(msg)

    def _on_vehicle_command_ack(self, msg: VehicleCommandAck) -> None:
        self._last_cmd_ack = msg
        if msg.result == VehicleCommandAck.VEHICLE_CMD_RESULT_ACCEPTED:
            return

        result_map = {
            VehicleCommandAck.VEHICLE_CMD_RESULT_ACCEPTED: "ACCEPTED",
            VehicleCommandAck.VEHICLE_CMD_RESULT_TEMPORARILY_REJECTED: "TEMP_REJECTED",
            VehicleCommandAck.VEHICLE_CMD_RESULT_DENIED: "DENIED",
            VehicleCommandAck.VEHICLE_CMD_RESULT_UNSUPPORTED: "UNSUPPORTED",
            VehicleCommandAck.VEHICLE_CMD_RESULT_FAILED: "FAILED",
            VehicleCommandAck.VEHICLE_CMD_RESULT_IN_PROGRESS: "IN_PROGRESS",
            VehicleCommandAck.VEHICLE_CMD_RESULT_CANCELLED: "CANCELLED",
        }
        result_str = result_map.get(int(msg.result), str(int(msg.result)))
        self.get_logger().warning(
            f"cmd_ack command={int(msg.command)} result={result_str} "
            f"param1={int(msg.result_param1)} param2={int(msg.result_param2)}"
        )

    def _publish_offboard_setpoints(self) -> None:
        now = self._timestamp_us()

        offboard_mode = OffboardControlMode()
        offboard_mode.timestamp = now
        offboard_mode.position = False
        offboard_mode.velocity = True
        offboard_mode.acceleration = False
        offboard_mode.attitude = False
        offboard_mode.body_rate = False
        self.offboard_mode_pub.publish(offboard_mode)

        target = self._get_target_velocity_setpoint()

        sp = TrajectorySetpoint()
        sp.timestamp = now
        nan = float("nan")
        sp.position = [nan, nan, nan]
        sp.velocity = [float(target.vx), float(target.vy), float(target.vz)]
        sp.acceleration = [nan, nan, nan]
        sp.jerk = [nan, nan, nan]
        sp.yaw = float("nan")
        sp.yawspeed = float(target.yawspeed)
        self.traj_sp_pub.publish(sp)

    def _publish_vehicle_command(self, command: int, **params: float) -> None:
        msg = VehicleCommand()
        msg.timestamp = self._timestamp_us()
        msg.command = int(command)
        msg.param1 = float(params.get("param1", 0.0))
        msg.param2 = float(params.get("param2", 0.0))
        msg.param3 = float(params.get("param3", 0.0))
        msg.param4 = float(params.get("param4", 0.0))
        msg.param5 = float(params.get("param5", 0.0))
        msg.param6 = float(params.get("param6", 0.0))
        msg.param7 = float(params.get("param7", 0.0))
        msg.target_system = (
            int(self._vehicle_status.system_id)
            if self._vehicle_status is not None
            else 1
        )
        msg.target_component = 1
        msg.source_system = 1
        msg.source_component = 1
        msg.from_external = True
        self.vehicle_cmd_pub.publish(msg)

    def _try_set_offboard(self) -> None:
        # MAV_CMD_DO_SET_MODE (176)
        # param1: MAV_MODE_FLAG_CUSTOM_MODE_ENABLED (1)
        # param2: PX4_CUSTOM_MAIN_MODE_OFFBOARD (6)
        self._publish_vehicle_command(
            VehicleCommand.VEHICLE_CMD_DO_SET_MODE, param1=1.0, param2=6.0
        )

    def _try_arm(self) -> None:
        self._publish_vehicle_command(
            VehicleCommand.VEHICLE_CMD_COMPONENT_ARM_DISARM, param1=1.0
        )

    def _has_valid_local_position(self) -> bool:
        if self._local_pos is None:
            return False
        if not (self._local_pos.xy_valid and self._local_pos.z_valid):
            return False
        if any(
            math.isnan(v)
            for v in (self._local_pos.x, self._local_pos.y, self._local_pos.z)
        ):
            return False
        return True

    def _get_target_velocity_setpoint(self) -> NedVelocitySetpoint:
        if self._target_twist is None:
            raise RuntimeError("Target twist not available")

        twist_stamped = self._target_twist
        twist = twist_stamped.twist

        vx_enu = float(twist.linear.x)
        vy_enu = float(twist.linear.y)
        vz_enu = float(twist.linear.z)
        vx_ned, vy_ned, vz_ned = transform_vector_enu_to_ned(vx_enu, vy_enu, vz_enu)

        # Twist.angular.z is interpreted as ENU yaw rate (rad/s) about +Z (Up).
        # PX4 expects yaw speed about +Z in NED (Down), so the sign flips.
        yawspeed_ned = -float(twist.angular.z)

        return NedVelocitySetpoint(
            vx=vx_ned, vy=vy_ned, vz=vz_ned, yawspeed=yawspeed_ned
        )

    def _publish_ros_odometry(self, msg: VehicleOdometry) -> None:
        if msg.pose_frame != VehicleOdometry.POSE_FRAME_NED:
            return

        x_ned = float(msg.position[0])
        y_ned = float(msg.position[1])
        z_ned = float(msg.position[2])
        x_enu, y_enu, z_enu = transform_vector_ned_to_enu(x_ned, y_ned, z_ned)

        q_ned = Quaternion(
            w=float(msg.q[0]),
            x=float(msg.q[1]),
            y=float(msg.q[2]),
            z=float(msg.q[3]),
        )
        q_enu = transform_orientation_ned_to_enu(q_ned)

        odom = Odometry()
        odom.header.stamp = self.get_clock().now().to_msg()
        odom.header.frame_id = "map"
        odom.child_frame_id = "base_link"
        odom.pose.pose.position.x = x_enu + self.spawn_x_enu
        odom.pose.pose.position.y = y_enu + self.spawn_y_enu
        odom.pose.pose.position.z = z_enu + self.spawn_z_enu
        odom.pose.pose.orientation.w = float(q_enu.w)
        odom.pose.pose.orientation.x = float(q_enu.x)
        odom.pose.pose.orientation.y = float(q_enu.y)
        odom.pose.pose.orientation.z = float(q_enu.z)

        if msg.velocity_frame == VehicleOdometry.VELOCITY_FRAME_NED and not any(
            math.isnan(v) for v in msg.velocity
        ):
            vx_enu, vy_enu, vz_enu = transform_vector_ned_to_enu(
                float(msg.velocity[0]), float(msg.velocity[1]), float(msg.velocity[2])
            )
            odom.twist.twist.linear.x = vx_enu
            odom.twist.twist.linear.y = vy_enu
            odom.twist.twist.linear.z = vz_enu

        self.odom_pub.publish(odom)

        if not self.viz_enabled:
            return

        if self._path_msg is None:
            self._path_msg = Path()
            self._path_msg.header.frame_id = odom.header.frame_id

        self._path_msg.header.stamp = odom.header.stamp
        pose = PoseStamped()
        pose.header = odom.header
        pose.pose = odom.pose.pose
        self._path_msg.poses.append(pose)
        if len(self._path_msg.poses) > 2000:
            self._path_msg.poses = self._path_msg.poses[-2000:]

        self.path_pub.publish(self._path_msg)

    def _tick(self) -> None:
        if self._landing_requested:
            self._tick_landing()
            return
        if self._target_twist is None:
            return

        self._publish_offboard_setpoints()
        self._setpoint_counter += 1

        if self._vehicle_status is None or not self._has_valid_local_position():
            return
        if self._setpoint_counter < 20:
            return

        now = self._timestamp_us()

        if not self._vehicle_status.pre_flight_checks_pass:
            if (now - self._last_preflight_warn_us) > 2_000_000:
                self._last_preflight_warn_us = now
                self.get_logger().warning(
                    "Preflight checks not passing; check QGC link/estimator/sensors. "
                    "This can block arming and OFFBOARD."
                )
            return

        want_offboard = (
            self._vehicle_status.nav_state != VehicleStatus.NAVIGATION_STATE_OFFBOARD
        )
        want_arm = self._vehicle_status.arming_state != VehicleStatus.ARMING_STATE_ARMED

        if want_offboard and (now - self._last_offboard_request_us) > 1_000_000:
            self._try_set_offboard()
            self._last_offboard_request_us = now
            self.get_logger().info("Requested OFFBOARD mode")

        if want_arm and (now - self._last_arm_request_us) > 1_000_000:
            self._try_arm()
            self._last_arm_request_us = now
            self.get_logger().info("Requested ARM")

    def _tick_landing(self) -> None:
        status = self._vehicle_status
        if status is None or status.arming_state != VehicleStatus.ARMING_STATE_ARMED:
            return
        if status.nav_state == VehicleStatus.NAVIGATION_STATE_AUTO_LAND:
            return
        now = self._timestamp_us()
        if self._last_land_request_us is None or now - self._last_land_request_us >= 1_000_000:
            self._publish_vehicle_command(
                VehicleCommand.VEHICLE_CMD_NAV_LAND,
                param4=float("nan"), param5=float("nan"),
                param6=float("nan"), param7=float("nan"),
            )
            self._last_land_request_us = now
            self.get_logger().info("Requested LAND; offboard and arming suspended")


def main(args=None) -> None:
    rclpy.init(args=args)
    node = Px4Bridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        print(f"Node {node.get_name()} interrupted")
    except Exception as e:
        print(f"Node {node.get_name()} shut down error: {e}")
    finally:
        print(f"Node {node.get_name()} shut down.")


if __name__ == "__main__":
    main()
