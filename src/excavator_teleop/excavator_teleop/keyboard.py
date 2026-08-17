import select
import sys
import termios
import tty

import rclpy
from excavator_msgs.msg import ExcavatorCommand
from rclpy.node import Node


KEY_BINDINGS = {
    "w": ("throttle", 1.0),
    "s": ("throttle", -1.0),
    "a": ("steering", -1.0),
    "d": ("steering", 1.0),
    "j": ("swing", -1.0),
    "l": ("swing", 1.0),
    "i": ("boom", 1.0),
    "k": ("boom", -1.0),
    "u": ("stick", 1.0),
    "o": ("stick", -1.0),
    "n": ("bucket", 1.0),
    "m": ("bucket", -1.0),
}


class KeyboardTeleop(Node):
    def __init__(self):
        super().__init__("excavator_keyboard_teleop")
        self.publisher = self.create_publisher(
            ExcavatorCommand,
            "/excavator/command",
            10,
        )
        self.timer = self.create_timer(0.05, self.tick)
        self.settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
        self.get_logger().info(
            "WASD drive | J/L swing | I/K boom | U/O stick | "
            "N/M bucket | SPACE stop | E emergency stop | Q quit"
        )

    def close(self):
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.settings)

    def read_key(self):
        readable, _, _ = select.select([sys.stdin], [], [], 0.0)
        return sys.stdin.read(1).lower() if readable else ""

    def tick(self):
        command = ExcavatorCommand()
        command.header.stamp = self.get_clock().now().to_msg()
        key = self.read_key()

        if key == "q":
            raise KeyboardInterrupt
        if key == "e":
            command.emergency_stop = True
        elif key in KEY_BINDINGS:
            field_name, value = KEY_BINDINGS[key]
            setattr(command, field_name, value)

        self.publisher.publish(command)


def main(args=None):
    rclpy.init(args=args)
    node = KeyboardTeleop()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.close()
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
