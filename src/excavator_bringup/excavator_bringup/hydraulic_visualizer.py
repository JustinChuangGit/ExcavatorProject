import math

import rclpy
from rclpy.duration import Duration
from rclpy.node import Node
from tf2_ros import Buffer, TransformException, TransformListener
from visualization_msgs.msg import Marker, MarkerArray


HYDRAULIC_PAIRS = [
    ("hyd_01_lower_pin", "hyd_01_upper_pin"),
    ("hyd_02_lower_pin", "hyd_02_upper_pin"),
    ("hyd_03_lower_pin", "hyd_03_upper_pin"),
    ("hyd_04_lower_pin", "hyd_04_upper_pin"),
    ("hyd_05_lower_pin", "hyd_05_upper_pin"),
]

LINKAGE_SEGMENTS = [
    ("bucket_linkage_end", "bucket_linkage_end01"),
    ("bucket_linkage_end01", "bucket_linkage_end03"),
    ("bucket_linkage_end03", "bucket_linkage_end"),
]

def _lerp(start, end, amount):
    return tuple(
        start[index] + (end[index] - start[index]) * amount
        for index in range(3)
    )


class HydraulicVisualizer(Node):
    def __init__(self):
        super().__init__("excavator_hydraulic_visualizer")
        self._buffer = Buffer(cache_time=Duration(seconds=2.0))
        self._listener = TransformListener(self._buffer, self)
        self._publisher = self.create_publisher(
            MarkerArray,
            "/excavator/hydraulics",
            10,
        )
        self._timer = self.create_timer(1.0 / 20.0, self._publish)
        self._last_missing_warning_ns = 0

    def _point(self, frame):
        transform = self._buffer.lookup_transform(
            "base_link",
            frame,
            rclpy.time.Time(),
            timeout=Duration(seconds=0.01),
        )
        translation = transform.transform.translation
        return (translation.x, translation.y, translation.z)

    @staticmethod
    def _cylinder(marker_id, start, end, radius, color):
        delta = tuple(end[index] - start[index] for index in range(3))
        length = math.sqrt(sum(component * component for component in delta))
        if length < 0.01:
            return None

        direction = tuple(component / length for component in delta)
        # Quaternion rotating the Marker cylinder's +Z axis onto direction.
        quaternion = (-direction[1], direction[0], 0.0, 1.0 + direction[2])
        norm = math.sqrt(sum(component * component for component in quaternion))
        if norm < 1.0e-6:
            quaternion = (1.0, 0.0, 0.0, 0.0)
        else:
            quaternion = tuple(component / norm for component in quaternion)

        marker = Marker()
        marker.header.frame_id = "base_link"
        marker.ns = "excavator_hydraulics"
        marker.id = marker_id
        marker.type = Marker.CYLINDER
        marker.action = Marker.ADD
        marker.pose.position.x = (start[0] + end[0]) * 0.5
        marker.pose.position.y = (start[1] + end[1]) * 0.5
        marker.pose.position.z = (start[2] + end[2]) * 0.5
        marker.pose.orientation.x = quaternion[0]
        marker.pose.orientation.y = quaternion[1]
        marker.pose.orientation.z = quaternion[2]
        marker.pose.orientation.w = quaternion[3]
        marker.scale.x = radius * 2.0
        marker.scale.y = radius * 2.0
        marker.scale.z = length
        marker.color.r = color[0]
        marker.color.g = color[1]
        marker.color.b = color[2]
        marker.color.a = color[3]
        return marker

    def _publish(self):
        markers = MarkerArray()
        delete_all = Marker()
        delete_all.header.frame_id = "base_link"
        delete_all.action = Marker.DELETEALL
        markers.markers.append(delete_all)
        marker_id = 1

        try:
            for lower_frame, upper_frame in HYDRAULIC_PAIRS:
                lower = self._point(lower_frame)
                upper = self._point(upper_frame)
                barrel_end = _lerp(lower, upper, 0.62)
                rod_start = _lerp(lower, upper, 0.45)
                barrel = self._cylinder(
                    marker_id,
                    lower,
                    barrel_end,
                    0.052,
                    (0.12, 0.13, 0.14, 1.0),
                )
                marker_id += 1
                rod = self._cylinder(
                    marker_id,
                    rod_start,
                    upper,
                    0.028,
                    (0.72, 0.74, 0.76, 1.0),
                )
                marker_id += 1
                if barrel is not None:
                    markers.markers.append(barrel)
                if rod is not None:
                    markers.markers.append(rod)

            for start_frame, end_frame in LINKAGE_SEGMENTS:
                linkage = self._cylinder(
                    marker_id,
                    self._point(start_frame),
                    self._point(end_frame),
                    0.025,
                    (0.92, 0.55, 0.05, 1.0),
                )
                marker_id += 1
                if linkage is not None:
                    markers.markers.append(linkage)
        except TransformException as error:
            now_ns = self.get_clock().now().nanoseconds
            if now_ns - self._last_missing_warning_ns > 5_000_000_000:
                self.get_logger().warning(
                    f"Waiting for Unreal hydraulic frames: {error}"
                )
                self._last_missing_warning_ns = now_ns
            return

        stamp = self.get_clock().now().to_msg()
        for marker in markers.markers:
            marker.header.stamp = stamp
        self._publisher.publish(markers)


def main(args=None):
    rclpy.init(args=args)
    node = HydraulicVisualizer()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
