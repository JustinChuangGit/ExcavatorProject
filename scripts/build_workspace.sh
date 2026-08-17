#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIRECTORY="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROS_WORKSPACE="$(cd -- "${SCRIPT_DIRECTORY}/.." && pwd)"

# ROS 2 Humble on Ubuntu 22.04 uses the system Python 3.10. Keep Conda and
# pyenv shims from being selected by CMake or ROS package entry points.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
unset CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_PROMPT_MODIFIER CONDA_PYTHON_EXE
unset PYTHONHOME PYTHONPATH

source /opt/ros/humble/setup.bash
cd "$ROS_WORKSPACE"

rosdep install \
    --from-paths src \
    --ignore-src \
    --rosdistro humble \
    --skip-keys ament_python \
    -y

colcon build \
    --symlink-install \
    --cmake-args \
    -DPython3_EXECUTABLE=/usr/bin/python3 \
    -DPYTHON_EXECUTABLE=/usr/bin/python3 \
    -DPYTHON_INCLUDE_DIR=/usr/include/python3.10 \
    -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.10.so
