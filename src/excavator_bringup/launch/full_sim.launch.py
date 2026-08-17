from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def include_launch(file_name):
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("excavator_bringup"), "launch", file_name]
            )
        )
    )


def generate_launch_description():
    return LaunchDescription(
        [
            include_launch("sim.launch.py"),
            include_launch("rviz.launch.py"),
        ]
    )
