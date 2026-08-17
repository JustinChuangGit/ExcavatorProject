from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    package_share = Path(get_package_share_directory("excavator_bringup"))
    robot_description = (package_share / "urdf" / "excavator.urdf").read_text()
    rviz_config = str(package_share / "config" / "excavator.rviz")

    return LaunchDescription(
        [
            Node(
                package="robot_state_publisher",
                executable="robot_state_publisher",
                name="excavator_robot_state_publisher",
                output="screen",
                parameters=[{"robot_description": robot_description}],
                # Unreal publishes the exact moving bone transforms on /tf.
                # Keep robot_state_publisher for robot_description and fixed
                # sensor frames, but isolate its approximate movable chain.
                remappings=[
                    ("/tf", "/robot_state_publisher/tf"),
                    ("/tf_static", "/robot_state_publisher/tf_static"),
                ],
            ),
            Node(
                package="excavator_bringup",
                executable="hydraulic_visualizer",
                name="excavator_hydraulic_visualizer",
                output="screen",
            ),
            Node(
                package="rviz2",
                executable="rviz2",
                name="excavator_rviz",
                output="screen",
                arguments=["-d", rviz_config],
            ),
        ]
    )
