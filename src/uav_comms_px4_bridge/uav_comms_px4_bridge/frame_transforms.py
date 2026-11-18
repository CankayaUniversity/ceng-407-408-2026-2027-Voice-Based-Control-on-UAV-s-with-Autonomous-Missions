import math
from dataclasses import dataclass


@dataclass(frozen=True)
class Quaternion:
    w: float
    x: float
    y: float
    z: float

    def normalized(self) -> "Quaternion":
        n = math.sqrt(self.w * self.w + self.x * self.x + self.y * self.y + self.z * self.z)
        if n == 0.0:
            return Quaternion(1.0, 0.0, 0.0, 0.0)
        return Quaternion(self.w / n, self.x / n, self.y / n, self.z / n)

    def __mul__(self, other: "Quaternion") -> "Quaternion":
        # Hamilton product.
        w1, x1, y1, z1 = self.w, self.x, self.y, self.z
        w2, x2, y2, z2 = other.w, other.x, other.y, other.z
        return Quaternion(
            w=w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
            x=w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
            y=w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
            z=w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
        )


def quaternion_from_euler(roll: float, pitch: float, yaw: float) -> Quaternion:
    """
    Matches px4_ros_com::frame_transforms::utils::quaternion::quaternion_from_euler
    (ZYX / yaw-pitch-roll composition).
    """

    cy = math.cos(yaw * 0.5)
    sy = math.sin(yaw * 0.5)
    cp = math.cos(pitch * 0.5)
    sp = math.sin(pitch * 0.5)
    cr = math.cos(roll * 0.5)
    sr = math.sin(roll * 0.5)

    # ZYX order.
    return Quaternion(
        w=cr * cp * cy + sr * sp * sy,
        x=sr * cp * cy - cr * sp * sy,
        y=cr * sp * cy + sr * cp * sy,
        z=cr * cp * sy - sr * sp * cy,
    )


def quaternion_get_yaw(q: Quaternion) -> float:
    """
    Matches px4_ros_com::frame_transforms::utils::quaternion::quaternion_get_yaw.
    """

    q = q.normalized()
    q0, q1, q2, q3 = q.w, q.x, q.y, q.z
    return math.atan2(2.0 * (q0 * q3 + q1 * q2), 1.0 - 2.0 * (q2 * q2 + q3 * q3))


def quaternion_conjugate(q: Quaternion) -> Quaternion:
    return Quaternion(w=q.w, x=-q.x, y=-q.y, z=-q.z)


# Static quaternion (see px4_ros_com/include/px4_ros_com/frame_transforms.h)
NED_ENU_Q = quaternion_from_euler(math.pi, 0.0, math.pi / 2.0)


def transform_orientation_enu_to_ned(q_enu: Quaternion) -> Quaternion:
    """
    Matches transform_orientation(q, StaticTF::ENU_TO_NED): out = NED_ENU_Q * q.
    """

    return (NED_ENU_Q * q_enu).normalized()


def transform_orientation_ned_to_enu(q_ned: Quaternion) -> Quaternion:
    """
    Inverse of ENU->NED: q_enu = NED_ENU_Q^-1 * q_ned.
    """

    return (quaternion_conjugate(NED_ENU_Q) * q_ned).normalized()


def transform_vector_enu_to_ned(x_enu: float, y_enu: float, z_enu: float) -> tuple[float, float, float]:
    """
    Matches transform_static_frame(vec, StaticTF::ENU_TO_NED) for aligned ENU/NED axes:
      NED.x (North) = ENU.y (North)
      NED.y (East)  = ENU.x (East)
      NED.z (Down)  = -ENU.z (Up)
    """

    return float(y_enu), float(x_enu), float(-z_enu)


def transform_vector_ned_to_enu(x_ned: float, y_ned: float, z_ned: float) -> tuple[float, float, float]:
    """
    Inverse of ENU->NED:
      ENU.x (East)  = NED.y (East)
      ENU.y (North) = NED.x (North)
      ENU.z (Up)    = -NED.z (Down)
    """

    return float(y_ned), float(x_ned), float(-z_ned)
