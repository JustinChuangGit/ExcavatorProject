from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    device_id = LaunchConfiguration("device_id")
    deadzone = LaunchConfiguration("deadzone")

    return LaunchDescription(
        [
            DeclareLaunchArgument("device_id", default_value="0"),
            DeclareLaunchArgument("deadzone", default_value="0.12"),
            Node(
                package="joy",
                executable="joy_node",
                name="xbox_joy",
                output="screen",
                parameters=[
                    {
                        "device_id": device_id,
                        "deadzone": deadzone,
                        "autorepeat_rate": 20.0,
                    }
                ],
            ),
            Node(
                package="excavator_teleop",
                executable="xbox_controller",
                name="excavator_xbox_teleop",
                output="screen",
                parameters=[{"deadzone": deadzone}],
            ),
        ]
    )
