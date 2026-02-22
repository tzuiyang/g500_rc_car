# G500 RC Car — Claude Working Guide

## Project Overview

An FPV (First-Person View) RC car — stream live video and drive it remotely from any laptop.

---

## The Workflow (read this first)

### On the RPi (the car's brain) — run once per session
```bash
ssh yze@g500.local
cd ~/g500_rc_car
npm run g500:start
```
This starts everything on the car:
- `camera/webcam_stream.py` → MJPEG camera stream on `:8080`
- `server/index.js` → WebSocket + serial bridge + camera proxy on `:3000`
- *(future)* ROS 2 nodes

### On any laptop (the controller) — same command every time
```bash
npm run app
```
Opens the Electron FPV window, connects to `http://g500.local:3000`.

To override the RPi address (if mDNS not working):
```bash
G500_URL=http://192.168.1.107:3000 npm run app
```

### First-time laptop setup (one time only)
```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
npm install       # installs Electron (~100 MB) — cached forever after
npm run app
```

---

### Hardware Stack
| Component | Role |
|-----------|------|
| Raspberry Pi 5 | Main brain — networking, camera streaming, web server, ROS 2 orchestration |
| Arduino Nano | Microcontroller — low-level PWM/motor control, serial bridge to RPi |
| Innomaker U20CAM 1080p | Onboard camera — FPV video stream via MJPEG, V4L2/UVC USB webcam |
| 3D Printed Chassis | Custom frame (G500 style) |

### Software Architecture
```
Clients — dumb display shells, zero logic, zero hardware knowledge
    ├── Electron App  (any laptop)  ─┐
    └── Phone Browser               ─┤  all traffic goes through port 3000
                                     │
                                     ▼
                         Raspberry Pi 5  ← ALL intelligence lives here
                         │
                         ├── npm run g500:start  (scripts/start.sh)
                         │     ├── camera/webcam_stream.py  → :8080  (MJPEG source)
                         │     └── server/index.js          → :3000  (single public port)
                         │           ├── GET  /stream   ← proxies :8080 camera
                         │           ├── WS   /         ← drive commands
                         │           └── GET  /         ← serves phone UI
                         │
                         ├── Serial → Arduino Nano → L298N → DC Motor
                         │
                         └── ROS 2 (future)
                               ├── /g500/cmd_vel
                               ├── /g500/camera/image_raw
                               └── /g500/diagnostics
```

### Port Map
| Port | Service | Who connects |
|------|---------|-------------|
| `:8080` | Camera MJPEG (internal) | Only the server, via localhost proxy |
| `:3000` | Node.js server (public) | Electron, phone browser, everything |

### Key Goals
- [x] Project scaffolded and documented
- [x] Node.js + npm installed on RPi (v20.19.2 / 9.2.0)
- [x] Camera service working — `webcam_stream.py`, U20CAM at `/dev/video1`, stream confirmed in browser
- [x] `npm run g500:start` — single command starts all RPi services (camera + server)
- [x] `npm run app` — single command on any laptop opens FPV Electron UI, no per-machine setup
- [x] Electron is a dumb pass-through — zero logic, connects to `http://g500.local:3000`
- [x] Server scaffolded — WebSocket + serial bridge + camera proxy on port 3000
- [x] Arduino firmware scaffolded — JSON serial protocol + failsafe
- [x] Phone UI scaffolded — touch joystick + throttle
- [ ] `npm run g500:start` end-to-end test (camera + server both up, Electron connects)
- [ ] Hardware test: Arduino serial communication from server
- [ ] Driving controls wired up in Electron UI (joystick → WebSocket → server → Arduino)
- [ ] End-to-end drive test
- [ ] ROS 2 Humble — add to `scripts/start.sh` when ready
- [ ] rosbag2 recording + UI button
- [ ] Chassis design / print files

---

## Working Rules (MUST FOLLOW)

### 1. Topic MD Files
Every distinct problem, feature, or subsystem gets its own markdown file:
```
docs/[topic].md
```

**Each topic file MUST contain all of these sections:**

```
## Problem
## Theory
## Unit Tests  ← REQUIRED: how to test this subsystem in isolation
## Attempts    ← dated entries, never deleted
## Issue Log   ← every failure with ISSUE FORMAT (see below)
## Current Status
## Solution
```

### 2. Always Read Before Working
Before touching any topic, read its `.md` file first. This prevents dead-end loops.

### 3. Always Update After Working
After any attempt — success or failure — update the topic `.md` immediately.
Never leave an attempt undocumented.

### 4. No Dead-End Loops
If an approach is documented as failed, do NOT retry it. Pivot or ask the user.

### 5. Every Subsystem Must Have Tests
Before marking any feature as done, a test must exist and be documented.
No untested code is considered complete.

### 6. Update This File
When milestones are reached or architecture changes, update CLAUDE.md.

### 7. Electron Is a Dumb Pass-Through — NEVER add backend logic to it
**This is a permanent architectural rule, not a temporary shortcut.**

The Electron app is a zero-logic display shell. It does:
- Open a `BrowserWindow`
- Read `G500_URL` env var (default: `http://g500.local:3000`)
- Pass `streamUrl` and `wsUrl` to the renderer via IPC
- The renderer renders `<img src="${streamUrl}">` and will open `new WebSocket(wsUrl)` for controls

The Electron app does NOT and MUST NOT:
- Spawn any processes (`python3`, servers, scripts)
- Contain any business logic, state machines, or retry orchestration
- Know anything about the RPi, serial port, camera hardware, or ROS
- Talk to hardware directly

**Why:** The RPi is the single source of truth. Electron is just a window that could be
replaced by any browser at any time. Any laptop (or phone) should be able to view the stream
by pointing at `http://g500.local:8080/stream` without Electron at all.
All intelligence lives on the RPi — in `webcam_stream.py`, `server/index.js`, and ROS nodes.

**What belongs on the RPi (not Electron):**
- Camera stream server (`camera/webcam_stream.py`) — systemd auto-start
- Node.js WebSocket server (`server/index.js`) — systemd or Docker
- Serial bridge to Arduino
- ROS 2 nodes

---

## Issue Format (use this every time something fails)

When logging a failure in any `docs/[topic].md`, use this exact format:

```
### ISSUE-[number]: [Short title]
**Date:** YYYY-MM-DD
**Severity:** Critical / High / Medium / Low
**Symptom:** What the user or test actually saw (exact error message or behavior)
**Expected:** What should have happened
**Steps to reproduce:**
  1. ...
  2. ...
**Diagnosed cause:** (fill in when known, leave blank if unknown)
**Fix applied:** (fill in when resolved)
**Status:** Open / Fixed / Wont Fix
**rosbag:** (filename if a bag was captured during this issue, else N/A)
```

Example:
```
### ISSUE-001: Arduino not detected on /dev/ttyUSB0
**Date:** 2026-02-18
**Severity:** High
**Symptom:** Server logs "No Arduino detected. Running in mock mode."
**Expected:** Serial port opens at /dev/ttyUSB0
**Steps to reproduce:**
  1. docker compose up server
  2. Check server logs
**Diagnosed cause:** Nano appears as /dev/ttyACM0 on this RPi
**Fix applied:** Set SERIAL_PATH=/dev/ttyACM0 in .env
**Status:** Fixed
**rosbag:** N/A
```

---

## Unit Testing Requirements

Every subsystem must have tests. Tests live in `tests/[subsystem]/`.

### Test Standards
- Tests must be runnable with a single command (documented in the topic file)
- Tests must be runnable WITHOUT hardware where possible (use mocks/stubs)
- Every test must have a clear PASS / FAIL output — no ambiguous results
- Failed tests must be logged as an ISSUE in the relevant topic file

### Test Map — Required Tests Per Subsystem

| Subsystem | Test File | Run Command | Hardware Needed? |
|-----------|-----------|-------------|------------------|
| Server (Node.js) | `tests/server/server.test.js` | `npm test --workspace=server` | No |
| Serial bridge | `tests/server/serial.test.js` | `npm test --workspace=server` | No (mocked) |
| WebSocket protocol | `tests/server/ws.test.js` | `npm test --workspace=server` | No |
| Arduino firmware | `tests/firmware/protocol_test/` | Arduino Serial Monitor | Yes (Nano) |
| Camera service | `tests/camera/test_stream.py` | `python tests/camera/test_stream.py` | No (mocked) |
| ROS topics | `tests/ros/test_topics.py` | `pytest tests/ros/` | No (mock node) |
| rosbag record/stop | `tests/ros/test_bag.py` | `pytest tests/ros/` | No |
| UI controls | Manual test checklist | See `docs/web-ui.md` | No |

### How to Run All Tests
```bash
# JS unit tests (server)
npm test

# Python tests (camera + ROS)
pytest tests/

# Full integration test (requires hardware)
bash scripts/integration_test.sh
```

---

## ROS 2 Integration

### Version: ROS 2 Humble (LTS, supported until 2027)
Runs inside Docker on the RPi 5. All services communicate via ROS 2 DDS.

### Topic Map
| Topic | Type | Publisher | Subscriber |
|-------|------|-----------|------------|
| `/g500/cmd_vel` | `geometry_msgs/Twist` | Node.js server (rclnodejs) | Serial bridge node |
| `/g500/camera/image_raw` | `sensor_msgs/Image` | Camera node (OpenCV/V4L2) | rosbag2 |
| `/g500/camera/compressed` | `sensor_msgs/CompressedImage` | Camera node | MJPEG streamer |
| `/g500/arduino/status` | `std_msgs/String` (JSON) | Serial bridge node | rosbag2, diagnostics |
| `/g500/diagnostics` | `diagnostic_msgs/DiagnosticArray` | All nodes | rosbag2 |
| `/g500/bag/control` | `std_msgs/String` | Node.js server | Bag recorder node |

### cmd_vel Mapping
```
Twist.linear.x   → throttle  (-1.0 to 1.0)
Twist.angular.z  → steering  (-1.0 to 1.0, left positive)
All other fields → 0
```

### ROS 2 Docker Service
Added as a fourth container in docker-compose:
- `ros-core` — runs `ros2 launch g500 g500_bringup.launch.py`
- Includes serial bridge node, bag recorder node, diagnostics aggregator
- Node.js server connects as a rclnodejs node

See `docs/ros-integration.md` for full setup details.

---

## rosbag2 Logging

### Purpose
Record all ROS 2 topics to disk so every hardware or software issue can be
replayed and analyzed after the fact. This is the primary debugging tool.

### Storage
```
logs/
└── bags/
    └── YYYY-MM-DD_HH-MM-SS/   ← one folder per recording session
        ├── metadata.yaml
        └── *.db3
```
Bags are gitignored (too large). Back up to external storage manually.

### UI Control
The phone UI has a **REC button** (top-left corner):
- **Tap once** → sends `{"type":"bag","action":"start"}` over WebSocket
- Server calls `ros2 bag record -a -o logs/bags/<timestamp>` as a subprocess
- Button turns red + shows elapsed time
- **Tap again** → sends `{"type":"bag","action":"stop"}`
- Server kills the bag process, bag is finalized

### What Gets Recorded
By default, all topics (`-a` flag):
- Drive commands, camera frames, Arduino status, diagnostics

### Analysis Commands
```bash
# List recorded bags
ls logs/bags/

# Inspect a bag
ros2 bag info logs/bags/<session-name>

# Play back a bag (replays all topics)
ros2 bag play logs/bags/<session-name>

# Plot a topic value over time (requires rqt)
rqt

# Export cmd_vel to CSV for spreadsheet analysis
python scripts/bag_to_csv.py logs/bags/<session-name> /g500/cmd_vel

# Check for gaps / dropped frames
python scripts/bag_inspect.py logs/bags/<session-name>
```

### Correlating Issues to Bags
Every ISSUE in a topic doc should reference the bag filename if one was recorded.
This lets us replay exact conditions that caused a bug.

---

## Environment & Portability

- **Docker Compose** — full production stack on RPi (`docker compose up -d`)
- **npm workspaces** — JS dev tooling (`npm run dev`, `npm test`)
- **ROS 2 Humble** — runs inside Docker, all inter-service comms go through ROS topics

### .env File (create locally, never commit)
```
SERIAL_PATH=/dev/ttyUSB0
SERIAL_BAUD=115200
ROS_DOMAIN_ID=42
BAG_DIR=/home/pi/g500_rc_car/logs/bags
```

See `docs/docker-setup.md` for environment setup progress.

---

## Repository Structure
```
g500_rc_car/
├── CLAUDE.md                        ← this file
├── README.md                        ← user-facing setup guide
├── package.json                     ← npm workspace root
├── .gitignore
│
├── docs/                            ← one .md per topic/subsystem
│   ├── docker-setup.md
│   ├── raspberry-pi-setup.md
│   ├── arduino-firmware.md
│   ├── camera-streaming.md
│   ├── serial-communication.md
│   ├── web-ui.md
│   ├── ros-integration.md
│   └── electron-app.md              ← Electron desktop FPV UI
│
├── server/                          ← Node.js backend (RPi)
│   ├── package.json
│   └── index.js
│
├── ui/                              ← Phone FPV controller (plain HTML)
│   ├── package.json
│   └── index.html
│
├── electron/                        ← Electron desktop app
│   ├── package.json
│   ├── main.js                      ← main process: spawns camera, opens window
│   ├── preload.js                   ← contextBridge IPC
│   └── index.html                   ← FPV driving UI renderer
│
├── camera/                          ← Python camera service
│   ├── requirements.txt
│   └── webcam_stream.py             ← U20CAM 1080p (OpenCV/V4L2) MJPEG server
│
├── ros/                             ← ROS 2 packages (NEW)
│   └── g500/
│       ├── package.xml
│       ├── setup.py
│       └── g500/
│           ├── serial_bridge_node.py
│           ├── bag_recorder_node.py
│           └── diagnostics_node.py
│
├── firmware/                        ← Arduino Nano
│   └── g500_nano/
│       └── g500_nano.ino
│
├── tests/                           ← All unit + integration tests (NEW)
│   ├── server/
│   │   ├── server.test.js
│   │   ├── serial.test.js
│   │   └── ws.test.js
│   ├── camera/
│   │   └── test_stream.py
│   ├── ros/
│   │   ├── test_topics.py
│   │   └── test_bag.py
│   └── firmware/
│       └── protocol_test/           ← Arduino test sketch
│           └── protocol_test.ino
│
├── docker/
│   ├── Dockerfile.server
│   ├── Dockerfile.camera
│   ├── Dockerfile.ros               ← NEW
│   └── docker-compose.yml
│
├── scripts/
│   ├── start.sh                     ← `npm run g500:start` — boots all RPi services
│   ├── bag_to_csv.py                ← Export bag topic to CSV
│   ├── bag_inspect.py               ← Check bag health/gaps
│   └── integration_test.sh          ← Full hardware integration test
│
└── logs/                            ← gitignored
    └── bags/
        └── .gitkeep
```

---

## Current Progress Log

| Date | Event |
|------|-------|
| 2026-02-18 | Project initialized, CLAUDE.md and directory structure created |
| 2026-02-18 | Full code scaffold: server, UI, camera service, Arduino firmware, Docker |
| 2026-02-18 | CLAUDE.md updated: added unit testing rules, issue format, ROS 2 integration, rosbag logging |
| 2026-02-22 | Hardware first contact: USB webcam detected (Innomaker U20CAM 1080p) |
| 2026-02-22 | Node.js v20.19.2 + npm 9.2.0 installed on RPi via apt |
| 2026-02-22 | `camera/webcam_stream.py` created — OpenCV + Flask MJPEG server for USB webcam |
| 2026-02-22 | Electron desktop app scaffolded — `electron/` workspace, `npm run app` wired up |
| 2026-02-22 | Camera stream debugged through 6 issues (lag, obsensor error, V4L2 backend, device index) |
| 2026-02-22 | **Camera stream WORKING** — browser confirmed at http://192.168.1.107:8080/stream |
| 2026-02-22 | U20CAM confirmed at `/dev/video1` on RPi 5 (not `/dev/video0` — RPi ISP nodes occupy lower numbers) |
| 2026-02-22 | Electron v20.18.3 installed; `docs/electron-app.md` and `docs/camera-streaming.md` updated |
| 2026-02-22 | **Architecture locked**: Electron is a dumb pass-through, all logic on RPi |
| 2026-02-22 | `npm run g500:start` wired up (`scripts/start.sh` — starts camera + server) |
| 2026-02-22 | Electron connects to single port 3000 via `G500_URL` env var (server proxies camera) |

---

## Hardware Notes

- **RPi 5** runs Raspberry Pi OS (64-bit). Ensure USB serial is enabled.
- **Arduino Nano** connected via USB to RPi 5. Serial baud 115200.
- **Camera** — Innomaker U20CAM 1080p. USB 2.0 UVC webcam. Detected as `/dev/video0`. Streams via `camera/webcam_stream.py`.
- **ESC** — type TBD based on motor selection.
- **Steering servo** — standard 50 Hz PWM, 1000–2000 µs.

---

## Contact / Repo
- GitHub: upload project root; `.gitignore` covers `node_modules/`, `logs/`, build artifacts, secrets.
- Never commit `.env` files.
