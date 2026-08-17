import math

import rclpy
from excavator_msgs.msg import ExcavatorCommand
from rclpy.node import Node
from sensor_msgs.msg import Joy
from std_msgs.msg import Bool, UInt8


def mix_drive_triggers(forward, reverse):
    """Return signed throttle: positive RT forward, negative LT reverse."""
    return max(-1.0, min(1.0, float(forward) - float(reverse)))


class XboxControllerTeleop(Node):
    """Translate a standard Xbox controller into normalized excavator commands."""

    def __init__(self):
        super().__init__("excavator_xbox_teleop")

        self.declare_parameter("deadzone", 0.12)
        self.declare_parameter("joy_timeout", 0.5)
        self.declare_parameter("enable_button", 4)
        self.declare_parameter("camera_button", 5)
        self.declare_parameter("emergency_stop_button", 1)
        self.declare_parameter("clear_emergency_stop_button", 6)
        self.declare_parameter("reset_button", 7)
        self.declare_parameter("camera_cycle_axis", 6)
        self.declare_parameter("camera_view_count", 5)
        self.declare_parameter("left_trigger_axis", 2)
        self.declare_parameter("right_trigger_axis", 5)

        self.deadzone = float(self.get_parameter("deadzone").value)
        self.joy_timeout = float(self.get_parameter("joy_timeout").value)
        self.enable_button = int(self.get_parameter("enable_button").value)
        self.camera_button = int(self.get_parameter("camera_button").value)
        self.emergency_stop_button = int(
            self.get_parameter("emergency_stop_button").value
        )
        self.clear_emergency_stop_button = int(
            self.get_parameter("clear_emergency_stop_button").value
        )
        self.reset_button = int(self.get_parameter("reset_button").value)
        self.camera_cycle_axis = int(
            self.get_parameter("camera_cycle_axis").value
        )
        self.camera_view_count = max(
            1,
            int(self.get_parameter("camera_view_count").value),
        )
        self.left_trigger_axis = int(
            self.get_parameter("left_trigger_axis").value
        )
        self.right_trigger_axis = int(
            self.get_parameter("right_trigger_axis").value
        )

        self.publisher = self.create_publisher(
            ExcavatorCommand,
            "/excavator/command",
            10,
        )
        self.subscription = self.create_subscription(
            Joy,
            "/joy",
            self.on_joy,
            10,
        )
        self.camera_publisher = self.create_publisher(
            UInt8,
            "/excavator/camera/select",
            10,
        )
        self.web_control_subscription = self.create_subscription(
            Bool,
            "/excavator/control/web_active",
            self.on_web_control,
            10,
        )
        self.timer = self.create_timer(0.05, self.publish_command)

        self.latest_joy = None
        self.latest_joy_time = None
        self.reported_layout = False
        self.timeout_reported = False
        self.active_camera_view = 0
        self.last_camera_axis_direction = 0
        self.web_control_active = False
        self.web_control_last_seen = None
        self.web_control_reported = False

        self.get_logger().info(
            "Excavator mapping: LB released = drive mode "
            "(left stick steer, RT/LT forward/reverse) | "
            "hold LB = dig mode (left stick swing/stick, "
            "right stick bucket/boom) | hold RB for camera | "
            "B E-stop | View clears E-stop | Start resets machine"
            " | D-pad left/right selects camera"
        )

    def on_joy(self, message):
        self.latest_joy = message
        self.latest_joy_time = self.get_clock().now()
        self.timeout_reported = False

        if not self.reported_layout:
            self.get_logger().info(
                f"Controller online: {len(message.axes)} axes, "
                f"{len(message.buttons)} buttons"
            )
            self.reported_layout = True

    def on_web_control(self, message):
        self.web_control_active = bool(message.data)
        self.web_control_last_seen = self.get_clock().now()

    def web_has_control(self):
        """Fail back to the local controller if the gateway disappears."""
        if not self.web_control_active or self.web_control_last_seen is None:
            return False
        age = (
            self.get_clock().now() - self.web_control_last_seen
        ).nanoseconds / 1e9
        return age <= 0.5

    def axis(self, index):
        if self.latest_joy is None or index >= len(self.latest_joy.axes):
            return 0.0

        value = float(self.latest_joy.axes[index])
        if not math.isfinite(value) or abs(value) <= self.deadzone:
            return 0.0

        scaled = (abs(value) - self.deadzone) / (1.0 - self.deadzone)
        return math.copysign(min(scaled, 1.0), value)

    def button(self, index):
        if self.latest_joy is None or index >= len(self.latest_joy.buttons):
            return False
        return bool(self.latest_joy.buttons[index])

    def trigger(self, index):
        """Xbox triggers report +1 released and -1 fully pressed."""
        if self.latest_joy is None or index >= len(self.latest_joy.axes):
            return 0.0
        value = float(self.latest_joy.axes[index])
        if not math.isfinite(value):
            return 0.0
        return min(max((1.0 - value) * 0.5, 0.0), 1.0)

    def joy_is_current(self):
        if self.latest_joy_time is None:
            return False
        age = (self.get_clock().now() - self.latest_joy_time).nanoseconds / 1e9
        return age <= self.joy_timeout

    def publish_command(self):
        command = ExcavatorCommand()
        command.header.stamp = self.get_clock().now().to_msg()

        if self.web_has_control():
            if not self.web_control_reported:
                self.get_logger().info(
                    "Web operator has control; local motion is suspended"
                )
                self.web_control_reported = True
            # A local emergency stop remains available even while the web
            # operator owns the motion lease.
            if self.joy_is_current() and self.button(
                self.emergency_stop_button
            ):
                command.emergency_stop = True
                self.publisher.publish(command)
            return
        if self.web_control_reported:
            self.get_logger().info(
                "Web control released; Xbox motion is active"
            )
            self.web_control_reported = False

        if not self.joy_is_current():
            if self.latest_joy_time is not None and not self.timeout_reported:
                self.get_logger().warning(
                    "Controller data timed out; publishing zero command"
                )
                self.timeout_reported = True
            self.publisher.publish(command)
            return

        command.emergency_stop = self.button(self.emergency_stop_button)
        command.clear_emergency_stop = self.button(
            self.clear_emergency_stop_button
        )
        command.reset_machine = self.button(self.reset_button)

        camera_axis = self.axis(self.camera_cycle_axis)
        camera_direction = 0
        if camera_axis >= 0.5:
            camera_direction = 1
        elif camera_axis <= -0.5:
            camera_direction = -1
        if camera_direction and not self.last_camera_axis_direction:
            self.active_camera_view = (
                self.active_camera_view + camera_direction
            ) % self.camera_view_count
            camera_message = UInt8()
            camera_message.data = self.active_camera_view
            self.camera_publisher.publish(camera_message)
            self.get_logger().info(
                f"Selected camera view {self.active_camera_view}"
            )
        self.last_camera_axis_direction = camera_direction

        # Reset is exclusive so a held stick cannot immediately drive the
        # machine away while it is being returned to its spawn pose.
        if command.reset_machine:
            self.publisher.publish(command)
            return

        # LB selects the implement controls. The digging directions are
        # inverted from the Linux joystick signs to match the requested
        # physical stick motion.
        if self.button(self.enable_button):
            command.swing = -self.axis(0)
            command.stick = self.axis(1)

            # RB temporarily gives the right stick to Unreal's camera. When
            # released, the same stick immediately returns to boom/bucket.
            if not self.button(self.camera_button):
                command.bucket = -self.axis(3)
                command.boom = self.axis(4)
        else:
            # Drive mode: the left stick steers, while the analog triggers
            # provide proportional travel. RT is forward and LT is reverse;
            # pressing both subtracts the two demands instead of choosing a
            # direction abruptly.
            command.steering = -self.axis(0)
            forward = self.trigger(self.right_trigger_axis)
            reverse = self.trigger(self.left_trigger_axis)
            command.throttle = mix_drive_triggers(forward, reverse)

        self.publisher.publish(command)


def main(args=None):
    rclpy.init(args=args)
    node = XboxControllerTeleop()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
