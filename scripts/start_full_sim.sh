#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIRECTORY="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROS_WORKSPACE="$(cd -- "${SCRIPT_DIRECTORY}/.." && pwd)"

# Keep ROS 2 Humble on Ubuntu's Python 3.10, independent of the user's
# interactive Conda or pyenv selection.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
unset CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_PROMPT_MODIFIER CONDA_PYTHON_EXE
unset PYTHONHOME PYTHONPATH

source /opt/ros/humble/setup.bash
source "${ROS_WORKSPACE}/install/setup.bash"

exec ros2 launch excavator_bringup full_sim.launch.py
