# G500 FPV RC Car

A remotely driven FPV RC car — stream live video and control the car from your phone browser.

**Hardware:** Raspberry Pi 5 · Arduino Nano · Innomaker U20CAM 1080p · L298N motor driver · 3D printed chassis
**Software:** Node.js server · Python camera service · Plain HTML phone UI · Docker Compose · ROS 2 Humble

---

## How it works

```
Phone Browser
    │  WebSocket (drive commands)  +  MJPEG stream (FPV video)
    ▼
Raspberry Pi 5  (Node.js server + ROS 2)
    │  USB Serial  (JSON commands at 115200 baud)
    ▼
Arduino Nano
    │  PWM (ENA D5) + direction (IN1 D8, IN2 D9)
    ▼
L298N Motor Driver  →  DC Motor

Raspberry Pi 5
    ◀──MJPEG────  Python camera service (U20CAM 1080p, /dev/video0)
```

1. Open the web UI on your phone — you see the FPV camera feed
2. Use the on-screen joystick and throttle to drive
3. Commands go via WebSocket → RPi → serial → Arduino → L298N → motor

---

## Hardware Wiring (confirmed working)

### L298N ↔ Arduino Nano ↔ Motor

| L298N Pin | Arduino Nano Pin | Purpose |
|-----------|-----------------|---------|
| ENA       | D5              | PWM speed (0–255) |
| IN1       | D8              | Direction bit A |
| IN2       | D9              | Direction bit B |
| OUT1      | —               | Motor + (red wire) |
| OUT2      | —               | Motor − (black wire) |
| 12V (VCC) | —               | Battery + |
| GND       | GND + Battery − | Common ground (required) |

```
  Battery 12V
  +──────────────────────────┐
  │                          │  L298N
  └─▶ 12V              OUT1 ──── Motor +
      GND  ◀──┐        OUT2 ──── Motor −
              │         ENA ◀── D5 (PWM)
      Nano    │         IN1 ◀── D8
      GND ────┘         IN2 ◀── D9
  Battery −
```

> **Critical:** GND must be shared between the Nano, L298N, and battery. Without common ground the motor will not respond.

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| [Node.js](https://nodejs.org) | ≥ 20 | Local dev server |
| [PlatformIO](https://platformio.org/install/cli) | latest | Flash Arduino firmware |
| [Docker](https://docs.docker.com/get-docker/) | ≥ 24 | RPi production stack |
| [Docker Compose](https://docs.docker.com/compose/) | v2 | Orchestrate services |
| Git | any | Clone repo |

Install PlatformIO CLI:
```bash
pip install platformio
```

---

## Flash the Arduino Firmware

### 1. Find your COM port

**Windows:** Check Device Manager → Ports. Clone Nanos show as `USB-SERIAL CH340 (COMx)`.
**Linux/RPi:** Run `ls /dev/ttyUSB* /dev/ttyACM*`

### 2. Set your port in platformio.ini

Edit [firmware/g500_nano/platformio.ini](firmware/g500_nano/platformio.ini):
```ini
upload_port = COM6        ; Windows example
; upload_port = /dev/ttyUSB0   ; Linux/RPi example
```

> **Note:** This Nano uses the **new optiboot bootloader** (post-2018 clone, CH340 chip).
> The config already uses `board = nanoatmega328new` — do not change it.
> If you get `stk500_recv: not responding`, see [docs/arduino-firmware.md](docs/arduino-firmware.md) ISSUE-001.

### 3. Flash

```bash
cd firmware/g500_nano
pio run --target upload
```

Expected output:
```
avrdude: 6952 bytes of flash written
avrdude: 6952 bytes of flash verified
========================= [SUCCESS] =========================
```

### 4. Verify with serial monitor

```bash
pio device monitor --port COM6 --baud 115200
```

Expected on boot:
```json
{"status":"ready","speed":5}
```
Then after 500ms with no commands:
```json
{"status":"failsafe"}
```
Both lines mean firmware is running correctly.

---

## Arduino Serial Commands

All commands at **115200 baud**, send via serial monitor or from RPi.

### Speed levels

Set maximum motor power before moving. Speed takes effect **immediately**, even while the motor is running.

| Command | Speed | PWM | Power |
|---------|-------|-----|-------|
| `1` | Level 1 | 28/255 | ~11% |
| `2` | Level 2 | 56/255 | ~22% |
| `3` | Level 3 | 85/255 | ~33% |
| `4` | Level 4 | 113/255 | ~44% |
| `5` | Level 5 | 141/255 | ~55% ← default |
| `6` | Level 6 | 170/255 | ~67% |
| `7` | Level 7 | 198/255 | ~78% |
| `8` | Level 8 | 226/255 | ~89% |
| `9` | Level 9 | 255/255 | 100% |

### Drive commands

| Command | Action |
|---------|--------|
| `F` | Forward at current speed level |
| `B` | Backward at current speed level |
| `S` | Stop (coast) |

### Example session

```
Send: 3       →  {"status":"speed","level":3}      (set slow speed)
Send: F       →  {"status":"forward","speed":3}    (motor spins slowly)
Send: 7       →  {"status":"speed","level":7}      (speeds up immediately)
Send: B       →  {"status":"backward","speed":7}   (reverses at level 7)
Send: S       →  {"status":"stop","speed":7}       (stops)
```

### JSON mode (for RPi / ROS)

```json
{"t": 0.75}
```
- `t` range: `-1.0` (full backward) → `0.0` (stop) → `1.0` (full forward)
- Magnitude is scaled by the current speed level — cannot exceed it

### Status replies

Every command gets a JSON reply on serial:

```json
{"status":"ready","speed":5}     ← boot
{"status":"forward","speed":5}   ← F command
{"status":"backward","speed":5}  ← B command
{"status":"stop","speed":5}      ← S command
{"status":"speed","level":3}     ← speed change
{"status":"failsafe"}            ← no command for 500ms → motor stopped
```

### Failsafe

If no command is received for **500 ms**, the motor stops automatically. This prevents the car running away if WiFi drops or the RPi crashes. Send any command to resume.

---

## Local Development (no RPi needed)

### 1. Clone and install

```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
npm install
```

### 2. Start the server

```bash
npm run dev
```

Server starts on **http://localhost:3000**.
If no Arduino is plugged in it runs in **mock mode** (warning logged, no crash).

### 3. Open the UI

Open **http://localhost:3000** in your browser.
Use `W A S D` or arrow keys for drive controls on desktop.

> Camera feed will show a "no camera" placeholder until the webcam is connected and the camera service is running.

---

## Raspberry Pi Deployment (full stack)

### 1. Set up RPi 5

```bash
# On RPi after flashing Raspberry Pi OS 64-bit:
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
sudo usermod -aG dialout $USER    # serial port access
# Log out and back in
```

### 2. Clone on the RPi

```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
```

### 3. Configure serial port

```bash
# Check which port the Nano appears on:
ls /dev/ttyUSB* /dev/ttyACM*

# Create .env if port is not /dev/ttyUSB0:
echo "SERIAL_PATH=/dev/ttyACM0" > .env
```

### 4. Start everything

```bash
cd docker
docker compose up -d
```

Starts:
- **server** — Node.js on port `3000` (UI + WebSocket + serial bridge)
- **camera** — Python/OpenCV MJPEG stream on port `8080` (internal, proxied by server)

### 5. Connect your phone

```
http://<rpi-ip>:3000
```

Find the IP: `hostname -I` on the RPi, or try `http://g500.local:3000`.

---

## Repository Structure

```
g500_rc_car/
├── README.md                        ← you are here
├── CLAUDE.md                        ← AI working guide + architecture + issue format
├── package.json                     ← npm workspace root
├── .gitignore
│
├── firmware/
│   ├── g500_nano/                   ← MAIN firmware (flash this to the car)
│   │   ├── platformio.ini           — board: nanoatmega328new, port: COM6
│   │   ├── src/main.cpp             — L298N driver, speed levels 1-9, failsafe
│   │   └── g500_nano.ino            — legacy reference only (not used)
│   └── motor_test/                  ← standalone motor test (ramp + manual)
│       ├── platformio.ini
│       └── src/main.cpp
│
├── server/                          ← Node.js backend (RPi)
│   ├── package.json
│   └── index.js                     — Express + WebSocket + serial bridge
│
├── ui/                              ← Phone FPV controller (plain HTML)
│   ├── package.json
│   └── index.html                   — touch joystick + throttle + FPV stream
│
├── electron/                        ← Desktop FPV UI (Electron)
│   ├── package.json
│   ├── main.js                      — spawns camera server, opens window
│   ├── preload.js
│   └── index.html                   — FPV driving UI with live MJPEG feed
│
├── camera/                          ← Python camera service (RPi)
│   ├── requirements.txt
│   └── webcam_stream.py             — U20CAM 1080p → MJPEG HTTP stream
│
├── docker/
│   ├── Dockerfile.server
│   ├── Dockerfile.camera
│   └── docker-compose.yml           — USB/serial passthrough configured
│
├── docs/                            ← per-topic progress + issue logs
│   ├── motor-first-test.md          ✅ SOLVED — L298N + DC motor confirmed working
│   ├── arduino-firmware.md          🔄 In Progress — main firmware written, testing
│   ├── serial-communication.md      ⬜ Not Started
│   ├── camera-streaming.md          🔄 In Progress — U20CAM scaffolded
│   ├── electron-app.md              🔄 In Progress — Electron UI scaffolded
│   ├── web-ui.md                    ⬜ Not Started
│   ├── ros-integration.md           ⬜ Architecture defined
│   ├── docker-setup.md              🔄 In Progress
│   └── raspberry-pi-setup.md        ⬜ Not Started
│
└── logs/bags/                       ← rosbag2 recordings (gitignored)
```

---

## npm Scripts

| Command | Description |
|---------|-------------|
| `npm install` | Install all dependencies (server + ui + electron) |
| `npm run app` | Launch Electron desktop FPV UI (starts camera server automatically) |
| `npm run dev` | Start server with hot-reload (nodemon) |
| `npm start` | Start server (production) |

## PlatformIO Commands

Run from the firmware directory:

| Command | Description |
|---------|-------------|
| `pio run --target upload` | Compile and flash to Nano |
| `pio device monitor --port COM6 --baud 115200` | Open serial monitor |
| `pio run` | Compile only (no flash) |

---

## Environment Variables (.env — never commit)

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | HTTP + WebSocket port |
| `SERIAL_PATH` | auto-detect | Arduino port e.g. `/dev/ttyUSB0` |
| `SERIAL_BAUD` | `115200` | Serial baud rate |
| `CAMERA_URL` | `http://camera:8080/stream` | Internal camera URL |
| `ROS_DOMAIN_ID` | `42` | ROS 2 DDS domain |

---

## Troubleshooting

**`stk500_recv: not responding` on upload**
→ Wrong bootloader target. Use `board = nanoatmega328new` in platformio.ini (already set).
→ See [docs/arduino-firmware.md](docs/arduino-firmware.md) ISSUE-001.

**`PermissionError: Access is denied` on COM port**
→ Another program has the port open (serial monitor, Arduino IDE).
→ Close it and retry.

**Motor doesn't spin despite serial output showing DONE**
→ Check common GND between Nano, L298N, and battery.
→ Check ENA is on D5 (PWM-capable pin).
→ See [docs/motor-first-test.md](docs/motor-first-test.md).

**Motor spins the wrong direction**
→ Swap OUT1 and OUT2 wires on the L298N (no code change needed).

**No serial connection to Arduino (from RPi)**
→ Run `ls /dev/ttyUSB* /dev/ttyACM*` and set `SERIAL_PATH` in `.env`.
→ Ensure user is in `dialout` group: `sudo usermod -aG dialout $USER`.

**Camera feed not showing**
→ Confirm the U20CAM is plugged in and detected: `v4l2-ctl --list-devices` should show `/dev/video0`.
→ Check camera service logs: `docker compose logs camera`.
→ Try accessing `http://<rpi-ip>:8080/stream` directly.

**Car doesn't respond to phone controls**
→ Check WebSocket status dot in the UI (top-left).
→ Check: `docker compose logs server`.

---

## Progress

| Subsystem | Status |
|-----------|--------|
| Motor — L298N + DC motor | ✅ Confirmed working |
| Arduino firmware — speed levels + failsafe | ✅ Flashed, boot confirmed |
| Camera — U20CAM 1080p MJPEG stream | 🔄 Scaffolded, live test pending |
| Electron desktop FPV UI | 🔄 Scaffolded, display setup pending |
| RPi server — WebSocket + serial bridge | 🔄 Scaffolded, hardware test pending |
| Phone UI — FPV joystick controller | 🔄 Scaffolded |
| Docker Compose — RPi deployment | 🔄 Scaffolded |
| ROS 2 — topic bridge + rosbag logging | ⬜ Architecture defined |

---

## Docs

Detailed notes, issue logs, and attempt history for each subsystem:

- [docs/motor-first-test.md](docs/motor-first-test.md) — L298N wiring, test results, ISSUE-001
- [docs/arduino-firmware.md](docs/arduino-firmware.md) — firmware protocol, speed levels, failsafe
- [docs/serial-communication.md](docs/serial-communication.md) — RPi ↔ Arduino serial bridge
- [docs/camera-streaming.md](docs/camera-streaming.md) — U20CAM → browser pipeline
- [docs/electron-app.md](docs/electron-app.md) — Electron desktop FPV UI
- [docs/web-ui.md](docs/web-ui.md) — phone FPV controller UI
- [docs/ros-integration.md](docs/ros-integration.md) — ROS 2 + rosbag2 logging plan
- [docs/docker-setup.md](docs/docker-setup.md) — Docker + npm environment
- [docs/raspberry-pi-setup.md](docs/raspberry-pi-setup.md) — RPi OS + config guide

Every issue is logged with: date, severity, symptom, diagnosed cause, fix applied, and rosbag reference.
See [CLAUDE.md](CLAUDE.md) for the full working conventions.
