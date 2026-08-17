# Excavator Simulator ROS 2 Architecture

## Objective

Build a ROS 2 controlled Unreal Engine 5.8 excavator simulator without
modifying the Marketplace asset directly. The first milestone is reliable,
two-way command and state communication. Camera streaming and operator UI
follow after the control loop is proven.

## Platform Decision

- Host OS: Ubuntu 22.04 amd64
- ROS distribution: ROS 2 Humble
- Unreal Engine: 5.8 on Linux
- ROS transport: `rosbridge_server` WebSocket on port 9090
- Rosbridge accepts any URL path on that dedicated port for compatibility with
  Unreal's Linux WebSockets backend
- Unreal transport implementation: project-owned `ExcavatorROS` C++ plugin
- Unreal gameplay adapter: child Blueprint of `BC_CE01`

ROS remains a separate process from Unreal. The Unreal plugin uses UE's
`WebSockets` and `Json` modules, so it does not link ROS libraries into the
Unreal process. This avoids ROS/Unreal compiler and ABI coupling and makes the
bridge replaceable later.

## Component Boundaries

```text
ROS 2 nodes
  teleop / autonomy / RViz / logging
                    |
                    | ROS topics
                    v
            rosbridge_server
                    |
                    | WebSocket JSON or CBOR
                    v
        UExcavatorROSBridgeComponent
                    |
                    | normalized command/state API
                    v
             BPI_ExcavatorControl
                    |
                    v
          BP_ROS_Excavator child
                    |
                    v
        Vendor BC_CE01 Blueprint
        Chaos vehicle and arm animation
```

### ROS workspace

`~/excavator_ros2_ws`

- `excavator_msgs`: project-owned command and state interfaces
- `excavator_bringup`: launch files and configuration
- `excavator_teleop`: keyboard/joystick command node
- Later: `excavator_description`, containing URDF and frame definitions

### Unreal project

- `Plugins/ExcavatorROS`: WebSocket transport, message parsing, watchdog, and
  Blueprint events
- `BPI_ExcavatorControl`: transport-independent normalized control interface
- `BP_ROS_Excavator`: child of the Marketplace `BC_CE01` Blueprint
- `WBP_ExcavatorOperator`: later operator camera/control UI

The Marketplace asset is treated as read-only.

## Topic Contract

### Commands into Unreal

| Topic | Type | Rate | Purpose |
|---|---|---:|---|
| `/excavator/command` | `excavator_msgs/msg/ExcavatorCommand` | 20-50 Hz | Complete normalized manual command |
| `/excavator/cmd_vel` | `geometry_msgs/msg/Twist` | 20-50 Hz | Standard base velocity compatibility |
| `/excavator/emergency_stop` | `std_msgs/msg/Bool` | Event + 2 Hz | Independent stop latch |

`ExcavatorCommand` fields:

```text
std_msgs/Header header
float32 throttle
float32 steering
float32 swing
float32 boom
float32 stick
float32 bucket
bool emergency_stop
bool clear_emergency_stop
bool reset_machine
```

All control values are normalized to `[-1.0, 1.0]`. Values are clamped again
inside Unreal.

### State published by Unreal

| Topic | Type | Initial rate | Purpose |
|---|---|---:|---|
| `/excavator/state` | `excavator_msgs/msg/ExcavatorState` | 20 Hz | Connection, command and simulator state |
| `/joint_states` | `sensor_msgs/msg/JointState` | 20 Hz | Swing, boom, stick and bucket state |
| `/excavator/odom` | `nav_msgs/msg/Odometry` | 20 Hz | Vehicle pose and velocity |
| `/tf` | `tf2_msgs/msg/TFMessage` | 20 Hz | Base pose plus live hydraulic and linkage pin frames |
| `/excavator/hydraulics` | `visualization_msgs/msg/MarkerArray` | 20 Hz | RViz telescoping cylinders and four-bar linkage |
| `/clock` | `rosgraph_msgs/msg/Clock` | frame rate | Simulation time, enabled in a later milestone |
| `/excavator/camera/image/compressed` | `sensor_msgs/msg/CompressedImage` | 10 Hz | Later ROS camera preview |

`ExcavatorState` fields:

```text
std_msgs/Header header
bool ros_connected
bool command_timed_out
bool emergency_stop
float32 throttle
float32 steering
float32 swing
float32 boom
float32 stick
float32 bucket
```

## Safety and Timing

- Xbox LB selects two exclusive modes. With LB released, left-stick X steers
  and the RT/LT triggers command proportional forward/reverse travel. With LB
  held, the common ISO excavator pattern maps left stick to cab swing and
  arm/stick, and right stick to bucket and boom
- Hydraulic axes are velocity commands with proportional response: stick
  magnitude controls joint speed, and zero input holds the current joint pose
  rather than returning it to its initial angle
- Boom, stick and bucket targets are clamped to a conservative mechanical
  envelope and taper their commanded velocity through the final 8 degrees,
  preventing the Marketplace cylinder rig from reaching unstable endpoints
- The adapter bypasses the vendor hydraulic Boolean flags and integrates each
  normalized ROS axis into a persistent numeric target on the animation
  instance before skeletal evaluation
- For the Level-1 non-digging proof of concept, physics collision is disabled on
  the animated upper-structure bones to prevent self-collision impulses; the
  chassis and wheels retain terrain collision
- Holding Xbox RB temporarily assigns the right stick to the Unreal camera and
  zeros the ROS boom/bucket axes
- Drive and dig commands are mutually exclusive, so vehicle travel is always
  zero while LB is held
- The Unreal adapter reapplies the latest ROS command before physics so the
  vendor Blueprint's direct controller mappings cannot steer the wheels
- Xbox B latches emergency stop; View/Menu explicitly clears it
- Xbox Start sends an edge-triggered reset to the initial vehicle transform,
  zeroes physics velocity and restores the initial excavator joint values
- Missing joystick messages produce zero commands within 0.5 seconds
- Command watchdog timeout: 0.5 seconds
- On timeout: immediately command every axis to zero
- Emergency stop: latched until `clear_emergency_stop` is explicitly set
- Clamp all normalized commands to `[-1.0, 1.0]`
- Apply ROS input only on Unreal's game thread
- Do not let network callbacks directly mutate actors or physics components
- Start with reliable WebSocket delivery and latest-command-wins queueing
- Reject non-finite numeric values
- Report connection and timeout state to both ROS and the operator UI

## Coordinate and Unit Convention

ROS uses meters and a right-handed body frame:

- `X`: forward
- `Y`: left
- `Z`: up

Unreal uses centimeters and its native handedness:

- `X`: forward
- `Y`: right
- `Z`: up

Conversions at the ROS boundary:

```text
ROS X meters = Unreal X centimeters / 100
ROS Y meters = -Unreal Y centimeters / 100
ROS Z meters = Unreal Z centimeters / 100
```

Rotation and angular-velocity conversion must also flip the appropriate Y-axis
sign. Conversion code lives in one utility class and is covered by unit tests.

## Frame Tree

Initial target:

```text
map
└── odom
    └── base_link
        ├── upper_body_link
        │   └── boom_link
        │       └── stick_link
        │           └── bucket_link
        └── camera_link
            └── camera_optical_frame
```

The Marketplace skeleton bone names are mapped inside the Unreal bridge to
stable ROS hydraulic pin names. A ROS visualization node connects the live pin
positions with barrel, rod, and linkage markers; RViz consumers never depend
on vendor bone names.

## Camera and Operator UI

The first camera is rendered locally in Unreal:

1. Attach a `SceneCapture2D` camera to the cab.
2. Render to a 1280x720 texture.
3. Display it in `WBP_ExcavatorOperator`.
4. Send UI controls through `BPI_ExcavatorControl`, the same interface used by
   ROS.
5. Add telemetry for connection, command timeout, speed, and joint angles.

ROS image publishing begins at 640x360 JPEG, 10 Hz. High-quality remote video
will use a dedicated video transport later instead of high-rate JSON.

## Implementation Milestones

### M1: Transport smoke test

- ROS 2 Humble and rosbridge installed
- Unreal plugin connects to `ws://127.0.0.1:9090/`
- Connection status is visible in Unreal logs
- ROS can publish a test command received by Unreal

Acceptance:

```text
ros2 topic pub --rate 20 /excavator/command \
  excavator_msgs/msg/ExcavatorCommand "{throttle: 0.25}"
```

The bridge reports valid commands and the watchdog reports timeout after the
publisher stops.

### M2: Drive control

- ROS throttle and steering call the same internal control path as keyboard
- Keyboard control remains available as a fallback
- Watchdog stops the vehicle within 0.5 seconds

### M3: Arm control

- Swing, boom, stick and bucket accept normalized commands
- Joint state is published with stable names
- Joint limits are enforced

### M4: State, odometry and TF

- Publish pose, velocity, joint state and frame transforms
- Verify units and handedness in RViz
- Add basic rosbag recording

### M5: Operator camera UI

- Cab camera and render target
- Full-screen UMG operator station
- Keyboard/joystick and ROS status
- Low-rate compressed ROS preview

### M6: Excavation behavior

- Whole-site, tiled 12.5 cm deformable height field
- Swept bucket-width cutting footprint
- Volume-conserving pickup, spill, carry, pour, and deposition
- Gravity-driven visible dump stream
- Angle-of-repose relaxation for spoil mounds
- Deferred per-tile collision updates for Chaos stability

The height field intentionally does not model caves or undercuts. A later
high-fidelity phase can add soil resistance, compaction, and traction without
changing the ROS command/state contract.

## Verification Checklist

- Unreal map check has no errors or warnings
- Unreal project builds from a clean shell
- ROS interfaces build with `colcon build`
- `ros2 interface show` finds project messages
- rosbridge launches on port 9090
- Unreal reconnects if rosbridge restarts
- Invalid messages do not crash Unreal
- Lost commands stop the machine
- Coordinate conversion tests pass
- Vendor assets remain unmodified
