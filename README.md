# ExcavatorProject

Complete collaborative source project for Justin Chuang's Mars excavator
simulator. It combines a ROS 2 Humble workspace with an Unreal Engine 5.8 Mars
world, excavator, deformable soil, ROS bridge, controller/keyboard teleoperation,
RViz visualization, C++ source, Blueprints, maps, textures, and editable source
art.

This guide is intentionally written so a new collaborator can give it to a
coding agent and have that agent prepare a fresh Ubuntu computer, build both
halves of the project, launch the simulator, and verify that it works.

## Read this first

- The validated platform is **Ubuntu 22.04 LTS x86_64**, **ROS 2 Humble**, and
  **Unreal Engine 5.8 on Linux**.
- The GitHub repository is private. Justin must add the new developer as a
  repository collaborator before clone access will work.
- The project uses Git LFS for Unreal assets. Install Git LFS before cloning and
  run `git lfs pull` after cloning.
- Unreal Engine itself is not stored here. Install a compatible Unreal Engine
  5.8 build separately.
- Generated ROS and Unreal build products are not stored here. They are rebuilt
  on each developer's computer.
- This repository is the complete **local ROS + Unreal simulator**. The separate
  mission-control website, Cloudflare tunnel, and Pixel Streaming signalling
  server are not included. The local launcher intentionally does not publish
  anything to the web.
- The repository contains project-specific third-party/Marketplace assets.
  Keep it private and do not redistribute or make it public without reviewing
  the licenses for every included asset.

## Setup-agent objective

A setup agent is finished only when all of the following are true:

1. The repository clones successfully and Git LFS replaces pointer files with
   the real Unreal assets.
2. All three ROS packages build successfully with `colcon`.
3. `UnrealTestEditor` compiles against Unreal Engine 5.8.
4. rosbridge listens on TCP port `9090`.
5. Unreal loads `/Game/ExcavatorSim/Maps/Mars_ExcavationSite`.
6. Unreal logs `Connected to rosbridge`.
7. `/excavator/state`, `/joint_states`, `/tf`, and the other simulator topics
   are visible from ROS.
8. A controller or keyboard command moves the excavator, and reset restores the
   excavator and Mars soil.

Do not treat a successful clone alone as a completed setup. Large files,
generated C++ modules, ROS dependencies, and the runtime connection all need
separate verification.

## Repository layout

```text
ExcavatorProject/
├── README.md
├── .gitattributes                 Git LFS rules for Unreal/binary assets
├── .gitignore                     ROS and Unreal generated-file exclusions
├── scripts/
│   ├── build_workspace.sh         Install ROS deps and build all packages
│   ├── start_local_unreal_ros.sh  Preferred local all-in-one launcher
│   ├── start_sim.sh               rosbridge + game-controller teleop
│   └── start_full_sim.sh          start_sim + robot state + RViz
├── src/
│   ├── excavator_msgs/            Project ROS messages
│   ├── excavator_teleop/          Keyboard and Xbox-style controller nodes
│   └── excavator_bringup/         Launch files, URDF, RViz, visualization
└── unreal/UnrealTest/
    ├── UnrealTest.uproject        Open this project with Unreal Engine 5.8
    ├── Config/                    Versioned Unreal project settings
    ├── Content/                   Mars world, maps, Blueprints, and assets
    ├── Docs/ROS_ARCHITECTURE.md   Detailed ROS/Unreal design
    ├── Plugins/ExcavatorROS/      Project-owned C++ rosbridge plugin
    ├── Scripts/                   Unreal Editor automation scripts
    ├── Source/                    UnrealTest C++ module
    └── SourceArt/                 Editable Mars-base and billboard sources
```

Important content paths:

```text
Unreal project:
  unreal/UnrealTest/UnrealTest.uproject

Main Mars map on disk:
  unreal/UnrealTest/Content/ExcavatorSim/Maps/Mars_ExcavationSite.umap

Main Mars map inside Unreal:
  /Game/ExcavatorSim/Maps/Mars_ExcavationSite

Custom Unreal plugin:
  unreal/UnrealTest/Plugins/ExcavatorROS

Editable Mars-base source:
  unreal/UnrealTest/SourceArt/MarsBase/unreal_export

Resume billboard source:
  unreal/UnrealTest/SourceArt/ResumeBillboard
```

Unreal World Partition also stores some level actors under
`Content/__ExternalActors__` and `Content/__ExternalObjects__`. Those files are
part of the world and must be committed with related map edits.

## 1. Computer and account requirements

### Tested software baseline

| Component | Required/tested value |
|---|---|
| Operating system | Ubuntu 22.04 LTS x86_64 |
| ROS | ROS 2 Humble desktop |
| Python | Ubuntu system Python 3.10 |
| Unreal Engine | 5.8, Linux editor |
| Git | Git with SSH or HTTPS GitHub access |
| Large files | Git LFS |
| Build tools | `build-essential`, CMake, `colcon`, `rosdep` |
| Runtime tools | `systemd --user`, `journalctl`, `ss`/`iproute2` |

Use Ubuntu's system Python for ROS. An active Conda, pyenv, or newer custom
Python can break ROS Humble's compiled Python modules. The project scripts
deliberately sanitize those environment variables.

The first clone transfers roughly 1.8 GB of Git LFS data. Unreal's generated
shaders, derived data, binaries, and intermediate build files require
additional free disk space. Unreal Engine itself needs substantially more disk
space and is installed separately.

### GitHub access

Justin must add the collaborator to the private repository:

```text
https://github.com/JustinChuangGit/ExcavatorProject
```

For SSH cloning, the collaborator must add an SSH public key to their GitHub
account. They can test it with:

```bash
ssh -T git@github.com
```

GitHub normally answers with an authentication-success message and notes that
it does not provide shell access. A repository `404` or `Repository not found`
usually means the GitHub account has not been granted access or the wrong SSH
identity is being used.

## 2. Install system prerequisites

### Install ROS 2 Humble

First configure the official ROS 2 apt repository for Ubuntu 22.04 and install
ROS 2 Humble Desktop according to the ROS installation guide. After the ROS apt
repository exists, the project needs at least these packages:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  file \
  git \
  git-lfs \
  iproute2 \
  openssh-client \
  python3-colcon-common-extensions \
  python3-rosdep2 \
  ros-humble-desktop \
  ros-humble-joy \
  ros-humble-robot-state-publisher \
  ros-humble-rosbridge-server \
  ros-humble-rviz2
```

Initialize rosdep once per computer:

```bash
sudo rosdep init
rosdep update
```

If `rosdep init` says it has already been initialized, do not delete the
existing configuration; continue with `rosdep update`.

Confirm the ROS installation in a fresh terminal:

```bash
source /opt/ros/humble/setup.bash
ros2 doctor --report
```

### Install Git LFS

```bash
git lfs install
git lfs version
```

`git lfs install` is a per-user setup step. Every collaborator needs to run it.

### Install Unreal Engine 5.8

Install a compatible **Unreal Engine 5.8 Linux** editor. A prebuilt Linux editor
or a source build can be used. Access to Epic's private Unreal Engine source
repository requires an Epic account linked to the collaborator's GitHub
account.

For a source installation, follow Epic's Linux build instructions for the 5.8
release: run the engine's `Setup.sh`, run `GenerateProjectFiles.sh`, and compile
the editor. Do not copy or commit the Unreal Engine directory into this
repository.

The local launcher automatically searches these locations:

```text
UnrealEditor available in PATH
~/Applications/UnrealEngine/Engine/Binaries/Linux/UnrealEditor
~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor
```

Any other location is supported through `UNREAL_EDITOR_PATH`.

The project uses the following engine/project plugins. `ExcavatorROS` is stored
in this repository; the others are included with a full Unreal Engine 5.8
installation:

- Chaos Vehicles
- Procedural Mesh Component
- WebSockets, JSON, and ImageWrapper engine modules
- Lidar Point Cloud
- Sun Position
- HDRI Backdrop
- Modeling Tools Editor Mode
- Datasmith import support
- Python Editor scripting
- Pixel Streaming 2 (enabled in the project, but not used by the local launcher)

## 3. Clone the repository and download all assets

SSH clone:

```bash
git lfs install
git clone git@github.com:JustinChuangGit/ExcavatorProject.git
cd ExcavatorProject
git lfs pull
```

HTTPS clone is also possible if the collaborator uses a GitHub credential
manager or personal access token:

```bash
git clone https://github.com/JustinChuangGit/ExcavatorProject.git
cd ExcavatorProject
git lfs pull
```

### Verify the LFS checkout

At the time this handoff guide was written, the repository contained 808 paths
tracked by Git LFS:

```bash
git lfs ls-files | wc -l
git lfs status
```

The main map should be a real Unreal package of about 22 MB, not a small text
pointer:

```bash
stat -c '%s %n' \
  unreal/UnrealTest/Content/ExcavatorSim/Maps/Mars_ExcavationSite.umap
file unreal/UnrealTest/Content/ExcavatorSim/Maps/Mars_ExcavationSite.umap
```

Expected map size for the current version:

```text
22050221 bytes
```

If the file begins with `version https://git-lfs.github.com/spec/v1`, the
checkout still contains an LFS pointer. Repair it with:

```bash
git lfs install
git lfs pull
git lfs checkout
```

Do not open and resave the Unreal project until the LFS download has completed.
Missing LFS payloads can make maps or assets appear corrupt or absent.

## 4. Build the ROS workspace

From the repository root:

```bash
./scripts/build_workspace.sh
```

The script:

1. Selects Ubuntu's system Python 3.10 instead of Conda/pyenv.
2. Sources `/opt/ros/humble/setup.bash`.
3. Runs `rosdep install --from-paths src --ignore-src`.
4. Builds all packages with `colcon build --symlink-install`.

Expected final line:

```text
Summary: 3 packages finished
```

The three packages are:

```text
excavator_msgs
excavator_teleop
excavator_bringup
```

Validate the build:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash
colcon list
ros2 interface show excavator_msgs/msg/ExcavatorCommand
ros2 interface show excavator_msgs/msg/ExcavatorState
```

The local `build/`, `install/`, and `log/` directories are expected after this
step. They are generated, ignored by Git, and must not be committed.

## 5. Compile the Unreal project

The project contains C++ modules, so a fresh computer must compile them against
its own Unreal Engine 5.8 installation. The first editor launch may offer to
build missing modules. An explicit command-line build is easier to diagnose.

From the repository root, adjust `UNREAL_ENGINE_ROOT` if needed:

```bash
export PROJECT_ROOT="$PWD"
export UNREAL_ENGINE_ROOT="$HOME/Applications/UnrealEngine"
export UNREAL_EDITOR_PATH="$UNREAL_ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor"

test -x "$UNREAL_EDITOR_PATH"

"$UNREAL_ENGINE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" \
  UnrealTestEditor \
  Linux \
  Development \
  "$PROJECT_ROOT/unreal/UnrealTest/UnrealTest.uproject" \
  -WaitMutex
```

The build creates ignored `Binaries/` and `Intermediate/` directories in the
project and its custom plugin. A successful build ends with a success result
from UnrealBuildTool.

To open the editor for development:

```bash
"$UNREAL_EDITOR_PATH" \
  "$PROJECT_ROOT/unreal/UnrealTest/UnrealTest.uproject"
```

Open this map in the Content Browser:

```text
/Game/ExcavatorSim/Maps/Mars_ExcavationSite
```

The first run can spend a long time compiling shaders and building local
derived data. That is expected. Do not commit `Binaries`, `Intermediate`,
`Saved`, or `DerivedDataCache`.

## 6. Preferred one-command local launch

Run this from an Ubuntu desktop terminal inside the cloned repository:

```bash
UNREAL_EDITOR_PATH="$HOME/Applications/UnrealEngine/Engine/Binaries/Linux/UnrealEditor" \
  ./scripts/start_local_unreal_ros.sh
```

If UnrealEditor is in `PATH` or one of the automatically searched locations,
the shorter form is enough:

```bash
./scripts/start_local_unreal_ros.sh
```

The launcher uses transient `systemd --user` services. It:

1. Stops any local services named for the public website, Pixel Streaming,
   Cloudflare tunnel, or watchdog.
2. Starts the full ROS stack as `excavator-ros-stack.service`.
3. Waits for rosbridge to open port `9090`.
4. Starts Unreal as `excavator-unreal-local.service`.
5. Loads the Mars map in game mode at 1600x900.
6. Waits for Unreal to report that the map finished loading.

The final output should report:

```text
ROS:             active
Unreal:          active
Website:         stopped
Pixel Streaming: stopped
Public tunnel:   stopped
Watchdog:        stopped for local mode
```

This launcher needs an active graphical Ubuntu desktop session. It preserves
the terminal's `DISPLAY`, `XAUTHORITY`, and `XDG_RUNTIME_DIR` when available.
It is not intended for a headless SSH session without GPU/display setup.

Although the launcher does not start a public tunnel, rosbridge's default
launch configuration listens on all interfaces at port `9090`. The host
firewall still controls LAN reachability. Use the manual local-only bridge
command below with `address:=127.0.0.1` if strict loopback binding is required.

## 7. Verify the running simulator

### Service and port checks

```bash
systemctl --user --no-pager --full status \
  excavator-ros-stack.service \
  excavator-unreal-local.service

ss -ltn 'sport = :9090'
```

Expected result: both services are active and a process is listening on port
`9090`.

### ROS checks

In another terminal, from the repository root:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash

ros2 node list
ros2 topic list | sort
ros2 topic echo --once /excavator/state
ros2 topic hz /excavator/state
```

Important topics include:

```text
/excavator/command
/excavator/state
/excavator/odom
/excavator/hydraulics
/joint_states
/tf
```

### Unreal log checks

```bash
journalctl --user -u excavator-unreal-local.service \
  --no-pager --lines=200 | \
  grep -E 'Load map complete|Connected to rosbridge|Mars soil reset|Machine reset'
```

Normal startup should contain:

```text
Load map complete /Game/ExcavatorSim/Maps/Mars_ExcavationSite
Connected to rosbridge
```

Some Unreal animation, TF extrapolation, or procedural-mesh cleanup warnings
can appear without breaking the simulation. Judge success using service state,
port `9090`, ROS topics, the rosbridge connection log, and actual controls.

### Reset verification

Press **Start** on the controller or **R** in the Unreal game. A ROS-only reset
test is:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash

ros2 topic pub --once /excavator/command \
  excavator_msgs/msg/ExcavatorCommand \
  '{clear_emergency_stop: true, reset_machine: true}'
```

Successful reset logs contain both:

```text
Machine reset to its initial transform
Mars soil reset
```

## 8. Manual launch and fallback paths

Use these steps if `systemd --user` is unavailable or if a developer wants to
see each process directly.

### Terminal 1: full ROS stack

```bash
cd /path/to/ExcavatorProject
./scripts/start_full_sim.sh
```

This starts rosbridge, Xbox-style controller teleop, robot-state publishing,
the hydraulic visualizer, and RViz.

### Terminal 2: Unreal game

```bash
cd /path/to/ExcavatorProject
export UNREAL_EDITOR_PATH="/path/to/UnrealEditor"

"$UNREAL_EDITOR_PATH" \
  "$PWD/unreal/UnrealTest/UnrealTest.uproject" \
  /Game/ExcavatorSim/Maps/Mars_ExcavationSite \
  -game \
  -windowed \
  -ResX=1600 \
  -ResY=900 \
  -nosound \
  -log
```

### Strictly loopback-only rosbridge

Instead of the full launcher, start only rosbridge like this:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash
ros2 launch excavator_bringup bridge.launch.py address:=127.0.0.1 port:=9090
```

Then start keyboard teleop in another terminal if no controller is available:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash
ros2 run excavator_teleop keyboard
```

## 9. Stop or cleanly restart the local simulator

Normal stop:

```bash
systemctl --user stop \
  excavator-unreal-local.service \
  excavator-ros-stack.service || true
```

Then start it again:

```bash
./scripts/start_local_unreal_ros.sh
```

If port `9090` is unexpectedly still occupied, inspect before terminating
anything:

```bash
ss -ltnp 'sport = :9090'
pgrep -af 'ros2|rosbridge|UnrealEditor|UnrealEditor-Cmd'
```

Stop the exact stale service or process shown by those commands. Do not use a
broad kill command that could terminate unrelated ROS or Unreal work. Both
`UnrealEditor` and `UnrealEditor-Cmd` should be checked because an editor
automation commandlet can survive an interrupted setup run.

Useful logs:

```bash
journalctl --user -u excavator-ros-stack.service -n 200 --no-pager
journalctl --user -u excavator-unreal-local.service -n 200 --no-pager
```

## 10. Controls

### Xbox-style controller mapping

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
Start                        reset machine and soil
Y                            leave/re-enter excavator
```

LB is the drive/dig mode selector. Releasing LB selects proportional travel
controls. Holding LB selects the common ISO excavator joystick pattern. While
RB is held, ROS zeros boom and bucket so the right stick can move the camera
without moving the implement.

The ROS node assumes the Linux `joy` package reports an Xbox-compatible axis
and button layout. Many Logitech and other controllers work when switched to
XInput/Xbox mode, but mappings can differ in DirectInput mode. Inspect the raw
device before changing code:

```bash
source /opt/ros/humble/setup.bash
ros2 run joy joy_node
ros2 topic echo /joy
```

If the axes/buttons differ, update the mapping in:

```text
src/excavator_teleop/excavator_teleop/xbox_controller.py
```

### Keyboard teleoperation

Start the keyboard node in a focused terminal:

```bash
source /opt/ros/humble/setup.bash
source ./install/setup.bash
ros2 run excavator_teleop keyboard
```

```text
W/S       throttle
A/D       steering
J/L       upper-body swing
I/K       boom
U/O       stick
N/M       bucket
Space     zero command
E         emergency stop
Q         quit keyboard teleop
```

Inside Unreal, **R** resets the world. **Y** or **E** leaves/re-enters the
excavator, **WASD** moves the walking operator, the mouse turns the camera, and
**Space** jumps.

Emergency stop is latched. Clear it explicitly with:

```bash
ros2 topic pub --once /excavator/command \
  excavator_msgs/msg/ExcavatorCommand \
  '{clear_emergency_stop: true}'
```

Disconnecting a controller produces zero commands. Unreal also stops the
machine if ROS commands disappear for 0.5 seconds.

## 11. Excavation behavior

The Mars site contains a 72 m red-regolith diggable surface. The bucket cuts a
12.5 cm-resolution tiled height field and visibly fills as soil is removed.
Raise the bucket and command bucket dump to create a falling regolith stream
and persistent spoil mound.

Soil volume is conserved between terrain, the bucket, displaced windrow, and
the dump stream. Collision updates are tiled and deferred until the excavator
clears a changed tile, avoiding a large Chaos impulse from recooking the whole
driving surface underneath the machine. Rocks are static scenery rather than
scoopable material.

Hydraulic joystick deflection commands joint velocity. A small deflection
moves a joint slowly; full deflection uses full speed. Returning a stick to
center commands zero velocity and holds the current swing, boom, stick, and
bucket angles rather than recentering them.

## 12. Architecture summary

This project does use WebSockets through rosbridge; it does **not** use WebRTC
for ROS controls. Pixel Streaming 2 can use WebRTC for remote video/input, but
the separate web/signalling stack is outside this repository and is not started
by the local launcher.

```text
Controller / keyboard / future autonomy
                  |
                  | ROS 2 topics
                  v
          rosbridge_server :9090
                  |
                  | WebSocket JSON/CBOR
                  v
       ExcavatorROS Unreal C++ plugin
                  |
                  | normalized command/state API
                  v
        BP_ROS_Excavator + Mars world
                  |
                  +----> excavator state / odometry / joints / TF
                  |
                  +----> RViz hydraulic visualization
```

Commands into Unreal:

| Topic | Type | Purpose |
|---|---|---|
| `/excavator/command` | `excavator_msgs/msg/ExcavatorCommand` | Normalized vehicle, arm, stop, and reset controls |
| `/excavator/cmd_vel` | `geometry_msgs/msg/Twist` | Standard base-velocity compatibility |
| `/excavator/emergency_stop` | `std_msgs/msg/Bool` | Independent stop latch |

State from Unreal:

| Topic | Type | Purpose |
|---|---|---|
| `/excavator/state` | `excavator_msgs/msg/ExcavatorState` | Connection, timeout, stop, and current command state |
| `/joint_states` | `sensor_msgs/msg/JointState` | Swing, boom, stick, and bucket state |
| `/excavator/odom` | `nav_msgs/msg/Odometry` | Vehicle pose and velocity |
| `/tf` | `tf2_msgs/msg/TFMessage` | Base and live mechanism transforms |
| `/excavator/hydraulics` | `visualization_msgs/msg/MarkerArray` | RViz cylinders and linkage |

ROS uses meters with X forward, Y left, and Z up. Unreal uses centimeters with
X forward, Y right, and Z up. The Unreal bridge owns the scale and handedness
conversion. See `unreal/UnrealTest/Docs/ROS_ARCHITECTURE.md` for the complete
topic contract, frame tree, timing, safety, and unit conventions.

## 13. Editing and collaborating safely

Before beginning work:

```bash
git switch main
git pull --rebase
git lfs pull
git status
git switch -c your-name/short-feature-name
```

After making changes, close Unreal or finish saving before checking Git:

```bash
git status
git lfs status
git add <specific-files>
git commit -m "Describe the change"
git push -u origin HEAD
```

Unreal `.uasset` and `.umap` files are binary and generally cannot be merged.
Coordinate ownership of `Mars_ExcavationSite` and major Blueprints so two
people do not edit the same binary asset at the same time. Map work may also
change files under `Content/__ExternalActors__` and
`Content/__ExternalObjects__`; review and commit the related files together.

Never commit these generated directories:

```text
build/
install/
log/
unreal/UnrealTest/Binaries/
unreal/UnrealTest/DerivedDataCache/
unreal/UnrealTest/Intermediate/
unreal/UnrealTest/Saved/
unreal/UnrealTest/Plugins/*/Binaries/
unreal/UnrealTest/Plugins/*/Intermediate/
```

Do not bypass Git LFS for `.uasset`, `.umap`, textures, meshes, audio, or PDF
source art. Check `.gitattributes` before introducing a new large binary
format.

## 14. Troubleshooting

### `Repository not found` or SSH permission denied

- Confirm Justin added the correct GitHub account as a collaborator.
- Accept the private-repository invitation.
- Run `ssh -T git@github.com`.
- Check `ssh -vT git@github.com` if multiple SSH keys are installed.
- Use the HTTPS clone URL with a credential manager if SSH is unavailable.

### Unreal assets or maps are missing, tiny, or invalid

```bash
git lfs install
git lfs pull
git lfs checkout
git lfs status
```

Check that the Mars `.umap` is 22,050,221 bytes. If GitHub reports an LFS quota
or bandwidth error, the repository owner must resolve that account-level issue;
normal Git commands cannot reconstruct missing binary payloads.

### `ros2: command not found`

```bash
source /opt/ros/humble/setup.bash
```

If `/opt/ros/humble/setup.bash` does not exist, ROS 2 Humble is not installed.

### ROS Python import or NumPy errors

Deactivate Conda/pyenv and use the supplied scripts. Confirm:

```bash
/usr/bin/python3 --version
```

The tested ROS Humble environment uses Ubuntu's Python 3.10.

### The ROS build cannot find dependencies

```bash
rosdep update
./scripts/build_workspace.sh
```

Read the first package that failed rather than only the final summary. The
script writes detailed logs under `log/latest_build/`.

### Unreal says modules are missing or built with another engine

- Confirm the editor is Unreal Engine 5.8, not 5.7 or an unrelated source tree.
- Run the explicit `Build.sh UnrealTestEditor ...` command in section 5.
- Review the first compiler error.
- If build outputs came from a different engine checkout, close Unreal and
  delete only the generated project/plugin `Binaries` and `Intermediate`
  directories, then rebuild. Never delete `Content`, `Config`, `Plugins`
  source, `Source`, or `SourceArt`.

### `UnrealEditor was not found`

Pass the complete binary path:

```bash
UNREAL_EDITOR_PATH="/absolute/path/to/UnrealEditor" \
  ./scripts/start_local_unreal_ros.sh
```

### Unreal will not open from a service or SSH session

Run the launcher from a terminal inside the logged-in graphical desktop. Check:

```bash
printf 'DISPLAY=%s\nXAUTHORITY=%s\nXDG_RUNTIME_DIR=%s\n' \
  "$DISPLAY" "$XAUTHORITY" "$XDG_RUNTIME_DIR"
```

For headless use, a GPU/display or virtual-display configuration is required
and is outside the default local workflow.

### Port `9090` is already in use

```bash
ss -ltnp 'sport = :9090'
systemctl --user status excavator-ros-stack.service --no-pager
pgrep -af 'ros2|rosbridge'
```

Stop the identified old service/process, then launch once. Repeatedly starting
the stack can create duplicate ROS/RViz nodes if an older launch tree survived.

### Unreal opens but never connects to ROS

1. Confirm port `9090` is listening.
2. Confirm `ros2 topic list` works after sourcing both setup files.
3. Check the Unreal service log for `Connected to rosbridge`.
4. Confirm the bridge URL remains `ws://127.0.0.1:9090/` in the Unreal
   `UExcavatorROSBridgeComponent` configuration.
5. Restart rosbridge and watch for Unreal's automatic reconnect.

### Controller is detected but controls are wrong

- Put Logitech/third-party hardware into XInput/Xbox mode when available.
- Inspect `/joy` values with `ros2 topic echo /joy`.
- Compare the device's axis/button indexes with
  `excavator_teleop/xbox_controller.py`.
- Use keyboard teleop until a device-specific mapping is added.

### Reset does not appear to work

Send the ROS reset command from section 7 and require both Unreal confirmation
lines: machine reset and Mars soil reset. A visual guess or a successful ROS
publish alone does not prove the full reset path executed.

## 15. Current validation record

Before this handoff, the packaged repository was validated by:

- Building all three ROS packages successfully on Ubuntu 22.04 / ROS Humble.
- Compiling the copied `UnrealTestEditor`, `UnrealTest` module, and
  `ExcavatorROS` plugin successfully against Unreal Engine 5.8.
- Confirming the main Mars map and all project assets are Git LFS tracked.
- Pushing all unique LFS payloads to the private GitHub repository.
- Performing a fresh private-repository clone with LFS smudging disabled.
- Pulling the 22,050,221-byte Mars `.umap` from LFS into that fresh clone and
  identifying it as a valid Unreal Engine package.

Because Unreal Engine and generated build outputs are intentionally excluded,
every new computer must still complete sections 2 through 7.

## Project scope and licensing

The ROS package manifests label the project-owned ROS packages as Apache-2.0.
That does not automatically relicense the entire repository or any bundled
Marketplace, third-party, resume, image, model, texture, or other content.
Treat the repository as private project material unless each asset's license
has been reviewed separately.

For deeper technical design, read:

```text
unreal/UnrealTest/Docs/ROS_ARCHITECTURE.md
```
