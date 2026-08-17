# ExcavatorProject

Complete collaborative source project for the ROS 2 Humble and Unreal Engine
5.8 excavator simulator.

## Repository contents

```text
src/                         ROS 2 messages, launch files, teleop, URDF, meshes
scripts/                     Build and local startup scripts
unreal/UnrealTest/Config     Unreal project configuration
unreal/UnrealTest/Content    Mars world, excavator, characters, and assets
unreal/UnrealTest/Plugins    ExcavatorROS source plugin
unreal/UnrealTest/Source     Unreal C++ project source
unreal/UnrealTest/SourceArt  Editable billboard source artwork
```

`SourceArt` also contains the Mars-base interchange files and the resume source
used by the in-world billboards. This repository is private and contains
project-specific licensed assets; keep access limited to collaborators who are
working on this project and do not make the repository public without reviewing
the licenses for those assets.

Generated `build`, `install`, `log`, `Binaries`, `Intermediate`, `Saved`, and
`DerivedDataCache` directories are intentionally excluded. They are recreated
locally and are not needed for collaboration. Unreal Engine itself is also not
stored in this repository; collaborators must install a compatible Unreal
Engine 5.8 build separately.

## Clone and prepare

Install Git LFS before cloning, then run:

```bash
git lfs install
git clone git@github.com:JustinChuangGit/ExcavatorProject.git
cd ExcavatorProject
git lfs pull
./scripts/build_workspace.sh
```

Requirements are Ubuntu 22.04, ROS 2 Humble, `rosdep`, `colcon`, Git LFS, and
Unreal Engine 5.8. The ROS build script installs declared ROS dependencies with
`rosdep` and builds the workspace with Ubuntu's system Python 3.10.

## Start locally without the website

Use the local launcher when you only want ROS and the Unreal simulation. It
stops the mission-control website, Pixel Streaming server, Cloudflare tunnel,
and availability watchdog before launching the Mars map, so nothing is made
public.

```bash
./scripts/start_local_unreal_ros.sh
```

The launcher locates UnrealEditor in `PATH`, `~/Applications/UnrealEngine`, or
`~/UnrealEngine`. For a different installation path, run:

```bash
UNREAL_EDITOR_PATH=/path/to/UnrealEditor ./scripts/start_local_unreal_ros.sh
```

It waits for rosbridge on port 9090 and for Unreal to finish loading the Mars
excavation map before reporting success. It does not start the website, Pixel
Streaming, Cloudflare tunnel, or public watchdog.

## Build

```bash
./scripts/build_workspace.sh
```

The script deliberately selects Ubuntu's Python 3.10 so an active Conda or
pyenv environment cannot break ROS 2 Humble's compiled Python modules.

## Start rosbridge

```bash
ros2 launch excavator_bringup bridge.launch.py
```

The Unreal plugin connects to:

```text
ws://127.0.0.1:9090/
```

## Xbox controller

Plug the controller in and start the complete simulator ROS stack:

```bash
./scripts/start_sim.sh
```

To launch rosbridge, controller input, robot-state publishing, live hydraulic
markers, and RViz together:

```bash
./scripts/start_full_sim.sh
```

Controls:

```text
LB released + left stick X  steer
LB released + RT             travel forward
LB released + LT             travel reverse
Hold LB + left stick X       cab swing
Hold LB + left stick Y       stick in/out
Hold LB + right stick X      bucket curl/dump
Hold LB + right stick Y      boom up/down
Hold RB                      temporarily use right stick for camera
B                            latch emergency stop
View/Menu                    clear emergency stop
Start                        reset machine to its spawn pose
```

LB is a drive/dig mode selector. Releasing LB selects proportional travel
controls; holding LB selects the common ISO excavator joystick pattern. While
RB is held, ROS zeros boom and bucket so the right stick can adjust the camera
without moving the implement. Start
publishes an exclusive reset command, so held controls cannot move the machine
during the reset. Disconnecting the controller also produces zero commands,
and Unreal independently stops the machine if ROS commands disappear for
0.5 seconds.

Hydraulic joystick deflection commands joint velocity: a small deflection moves
the joint slowly and full deflection moves it at full speed. Returning a stick
to center commands zero velocity and holds the swing, boom, stick, and bucket
at their current angles; it does not recenter the excavator.

## Mars excavation site

Start the ROS stack:

```bash
./scripts/start_sim.sh
```

Then run the Mars map:

```bash
"/path/to/UnrealEditor" \
  "$(pwd)/unreal/UnrealTest/UnrealTest.uproject" \
  /Game/ExcavatorSim/Maps/Mars_ExcavationSite \
  -game -windowed -ResX=1280 -ResY=720 -log
```

The complete 72 m red-regolith surface is diggable. The bucket cuts a
12.5 cm-resolution tiled height field and visibly fills as soil is removed.
The load remains attached while lifting or swinging. Raise the bucket, then
command bucket dump (hold LB and move the right stick right) to create a
falling regolith stream and a persistent spoil mound. Press Start on the Xbox
controller (or R on the keyboard) to restore the machine, bucket load, and
terrain to their initial state.

Soil volume is conserved between the terrain, bucket, displaced windrow, and
dump stream. Collision is also tiled and updates after the excavator clears a
changed tile, avoiding the large Chaos impulse caused by recooking the complete
driving surface underneath the machine. The rocks remain static scenery rather
than scoopable material.

Press **Y** on the Xbox controller (or Y/E on the keyboard) to leave the
excavator and walk around in third person. The left stick/WASD moves the
operator, the right stick/mouse turns the camera, and **A**/Space jumps. Press
Y again while within 5 m of the machine to re-enter it. ROS stays connected
while the operator is outside, but excavator motion commands are parked until
the machine is occupied again.

RViz receives the four main URDF joints plus live hydraulic pin frames from
Unreal. `excavator_bringup` draws the extending cylinders and bucket linkage on
`/excavator/hydraulics`; these replace the old linkage meshes that were rigidly
attached to the wrong URDF links.

## Keyboard teleoperation

In another terminal:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash
ros2 run excavator_teleop keyboard
```

Controls:

```text
W/S       throttle
A/D       steering
J/L       upper-body swing
I/K       boom
U/O       stick
N/M       bucket
Space     zero command
E         emergency stop
Q         quit
```

Emergency stop is latched in Unreal. Clear it explicitly:

```bash
ros2 topic pub --once /excavator/command \
  excavator_msgs/msg/ExcavatorCommand \
  "{clear_emergency_stop: true}"
```

## Topic smoke test

```bash
ros2 topic pub --rate 20 /excavator/command \
  excavator_msgs/msg/ExcavatorCommand \
  "{throttle: 0.25}"
```

Stop the publisher with `Ctrl+C`. Unreal's command watchdog must return all
axes to zero within 0.5 seconds.

The full system design is in:

```text
unreal/UnrealTest/Docs/ROS_ARCHITECTURE.md
```
