from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
)
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    port = LaunchConfiguration("port")
    address = LaunchConfiguration("address")

    rosbridge = IncludeLaunchDescription(
        AnyLaunchDescriptionSource(
            PathJoinSubstitution(
                [
                    FindPackageShare("rosbridge_server"),
                    "launch",
                    "rosbridge_websocket_launch.xml",
                ]
            )
        ),
        launch_arguments={
            "port": port,
            "address": address,
            # Unreal's Linux WebSockets backend may add an internal request
            # path. Rosbridge treats this value as a Tornado route regex.
            "url_path": "/.*",
        }.items(),
    )

    return LaunchDescription(
        [
            # rosbridge uses an /usr/bin/env python3 shebang. Keep Conda's
            # Python 3.13 out of ROS Humble's Python 3.10 processes.
            SetEnvironmentVariable(
                "PATH",
                "/opt/ros/humble/bin:/usr/local/sbin:/usr/local/bin:"
                "/usr/sbin:/usr/bin:/sbin:/bin",
            ),
            DeclareLaunchArgument("port", default_value="9090"),
            DeclareLaunchArgument("address", default_value="0.0.0.0"),
            rosbridge,
        ]
    )
