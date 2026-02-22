# Topic: ROS 2 Integration

**Status: `[ ] In Progress`**

---

## Problem

Wire all subsystems together through ROS 2 so that:
1. Drive commands from the phone flow through a standardized ROS topic pipeline
2. All data (commands, camera frames, Arduino status) is recorded to rosbag2 for debugging
3. Any node can be tested or replayed independently
4. Issues can be diagnosed by replaying a bag rather than guessing

---

## Theory

### Why ROS 2?
- Standardized message types (`Twist`, `Image`, `DiagnosticArray`) — battle-tested in robotics
- rosbag2: record everything, replay anything — best debugging tool available
- DDS transport: topics work across Docker containers on the same network
- Modular: camera node, serial bridge node, bag recorder are all independent processes
- ROS 2 Humble is LTS (supported until May 2027), builds on Ubuntu 22.04 (ARM64)

### ROS 2 vs ROS 1
ROS 2 is used (not ROS 1) because:
- Native Python 3 support
- No ROS master required (DDS peer-to-peer)
- Better Docker/container support
- Active development and community

### Architecture
```
Node.js server (rclnodejs)
    │ publishes
    ▼
/g500/cmd_vel  (geometry_msgs/Twist)
    │ subscribed by
    ▼
serial_bridge_node.py
    │ writes JSON to
    ▼
Arduino Nano (USB serial)


U20CAM 1080p (/dev/video0)
    │ captured by
    ▼
webcam_stream.py (OpenCV/V4L2)
    │ publishes (if ROS bridge added)
    ├── /g500/camera/image_raw      (sensor_msgs/Image)
    └── /g500/camera/compressed     (sensor_msgs/CompressedImage)


All topics
    │ subscribed by
    ▼
rosbag2 recorder
    │ saves to
    ▼
logs/bags/YYYY-MM-DD_HH-MM-SS/
```

### Docker Service: ros-core
A dedicated Docker container (`Dockerfile.ros`) based on `ros:humble` that:
- Sources `/opt/ros/humble/setup.bash`
- Builds the `g500` ROS package (`ros/g500/`)
- Runs `ros2 launch g500 g500_bringup.launch.py`
- Shares host network with other containers so DDS discovery works

### ROS 2 Package: g500
Located at `ros/g500/`. A standard Python-based ROS 2 package containing:

| Node | File | Purpose |
|------|------|---------|
| `serial_bridge` | `serial_bridge_node.py` | Subscribes to `/g500/cmd_vel`, writes JSON to Arduino |
| `bag_recorder` | `bag_recorder_node.py` | Subscribes to `/g500/bag/control`, starts/stops `ros2 bag record` |
| `diagnostics` | `diagnostics_node.py` | Aggregates health from all nodes, publishes to `/g500/diagnostics` |

### rclnodejs (Node.js ↔ ROS 2)
The Node.js server uses `rclnodejs` to join the ROS 2 network:
```js
const rclnodejs = require('rclnodejs');
const node = rclnodejs.createNode('g500_server');
const pub = node.createPublisher('geometry_msgs/msg/Twist', '/g500/cmd_vel');
// On WebSocket message:
pub.publish({ linear: { x: throttle }, angular: { z: steering } });
```

### rosbag2 Control Protocol
UI → WebSocket → server → ROS topic → bag_recorder_node

```
Start: {"type":"bag","action":"start"}
Stop:  {"type":"bag","action":"stop"}
```

Server response:
```
{"type":"bag","status":"recording","file":"2026-02-18_14-30-00"}
{"type":"bag","status":"stopped","file":"2026-02-18_14-30-00","duration_s":45}
```

---

## Unit Tests

### Run Command
```bash
# With ROS 2 sourced in environment (or inside Docker):
pytest tests/ros/

# Or inside the ros-core Docker container:
docker compose exec ros-core pytest /app/tests/ros/
```

### Tests Required

**`tests/ros/test_topics.py`**
- `test_cmd_vel_published` — mock Node.js publisher, verify serial bridge receives message
- `test_cmd_vel_clamped` — values outside -1..1 are rejected/clamped at bridge
- `test_failsafe_on_no_cmd` — if no cmd_vel for 500ms, bridge sends neutral command

**`tests/ros/test_bag.py`**
- `test_bag_start_creates_folder` — after start command, verify bag dir is created
- `test_bag_stop_finalizes` — after stop command, verify metadata.yaml exists
- `test_bag_contains_cmd_vel` — play bag back, verify /g500/cmd_vel topic present

### Manual Test — Verify Topics Running
```bash
# List active topics
ros2 topic list

# Echo drive commands in real time
ros2 topic echo /g500/cmd_vel

# Check publish rate
ros2 topic hz /g500/cmd_vel

# Check camera frame rate
ros2 topic hz /g500/camera/image_raw
```

---

## Attempts

### Attempt 1 — Architecture defined (2026-02-18)
**What we did:** Defined ROS 2 architecture, topic map, Docker service plan, rclnodejs integration approach.
**Result:** Documentation only — no code written yet.
**Next step:** Create `ros/g500/` package scaffold, `Dockerfile.ros`, add rclnodejs to server.

---

## Issue Log

_No issues yet._

---

## Current Status

`[ ] In Progress` — Architecture defined. Awaiting implementation.

### Next Actions
- [ ] Create `ros/g500/package.xml` and `setup.py`
- [ ] Write `serial_bridge_node.py`
- [ ] Write `bag_recorder_node.py`
- [ ] Write `diagnostics_node.py`
- [ ] Write `g500_bringup.launch.py`
- [ ] Create `docker/Dockerfile.ros`
- [ ] Add `ros-core` service to `docker-compose.yml`
- [ ] Add `rclnodejs` to server and publish cmd_vel
- [ ] Add rosbag start/stop WebSocket handler to server
- [ ] Add REC button to UI
- [ ] Write `tests/ros/test_topics.py`
- [ ] Write `tests/ros/test_bag.py`
- [ ] Run tests and verify

---

## Solution

_Not yet solved._
