from __future__ import annotations

from typing import Protocol

from geometry_msgs.msg import PoseStamped
from geometry_msgs.msg import TwistStamped
from nav_msgs.msg import Odometry


class SwarmAlgorithm(Protocol):
    def compute_setpoint(
        self,
        *,
        self_odom: Odometry,
        neighbors: list[Odometry],
        goal: PoseStamped | None,
        now_ns: int,
    ) -> TwistStamped: ...
