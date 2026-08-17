from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def include_launch(package_name, file_name):
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare(package_name), "launch", file_name]
            )
        )
    )


def generate_launch_description():
    return LaunchDescription(
        [
            include_launch("excavator_bringup", "bridge.launch.py"),
            include_launch("excavator_bringup", "xbox.launch.py"),
        ]
    )
