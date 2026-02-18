# G500 FPV RC Car

A remotely driven FPV RC car — stream live video and control the car from your phone browser, from anywhere.

**Hardware:** Raspberry Pi 5 · Arduino Nano · OAK-D Lite camera · 3D printed chassis
**Software:** Node.js server · Python camera service · Plain HTML phone UI · Docker Compose

---

## How it works

```
Phone Browser  ──WebSocket──▶  RPi 5 Node.js Server  ──USB Serial──▶  Arduino Nano  ──PWM──▶  ESC + Servo
                ◀──MJPEG────   Python camera service (OAK-D Lite)
```

1. Open the web UI on your phone — you see the FPV camera feed
2. Use the on-screen joystick (steering) and throttle slider to drive
3. Commands travel over WebSocket to the RPi, which forwards them to the Arduino over serial
4. The Arduino outputs PWM to the ESC (speed) and steering servo

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| [Node.js](https://nodejs.org) | ≥ 20 | Run server locally / build |
| [Docker](https://docs.docker.com/get-docker/) | ≥ 24 | Production deployment on RPi |
| [Docker Compose](https://docs.docker.com/compose/) | v2 (bundled with Docker Desktop) | Orchestrate services |
| [Arduino IDE](https://www.arduino.cc/en/software) | ≥ 2.x | Flash Arduino Nano firmware |
| Git | any | Clone this repo |

---

## Quickstart — Local Development (no hardware needed)

### 1. Clone and install

```bash
git clone <your-repo-url> g500_rc_car
cd g500_rc_car
npm install
```

### 2. Start the server

```bash
npm run dev
```

The server starts on **http://localhost:3000**.
If no Arduino is plugged in, it runs in **mock mode** (logs a warning, no serial errors).

### 3. Open the UI

Open **http://localhost:3000** in your browser.
Use `W A S D` or arrow keys to test drive controls on desktop.

> Camera feed will show a "no camera" placeholder until the OAK-D Lite is connected and the camera service is running.

---

## Raspberry Pi Deployment (full stack)

### 1. Set up the RPi 5

See [docs/raspberry-pi-setup.md](docs/raspberry-pi-setup.md) for the full guide. Short version:

```bash
# On the RPi after flashing Raspberry Pi OS (64-bit):
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
sudo usermod -aG dialout $USER   # serial port access
# Log out and back in
```

### 2. Clone the repo on the RPi

```bash
git clone <your-repo-url> g500_rc_car
cd g500_rc_car
```

### 3. Configure serial port (if needed)

By default Docker maps `/dev/ttyUSB0` for the Arduino. If your Nano appears as `/dev/ttyACM0`:

```bash
# Create a .env file in the project root:
echo "SERIAL_PATH=/dev/ttyACM0" > .env
```

Check which port the Arduino is on:
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

### 4. Start everything

```bash
cd docker
docker compose up -d
```

This builds and starts:
- **server** — Node.js on port `3000` (UI + WebSocket + serial bridge)
- **camera** — Python/DepthAI MJPEG stream on port `8080` (internal, proxied by server)

### 5. Connect your phone

Make sure your phone is on the same WiFi network as the RPi, then open:

```
http://<rpi-ip-address>:3000
```

Find the RPi's IP with `hostname -I` on the RPi, or try `http://g500.local:3000` if you set the hostname.

---

## Flash the Arduino Firmware

### Dependencies

Install the **ArduinoJson** library in Arduino IDE:
`Sketch → Include Library → Manage Libraries → search "ArduinoJson" → Install`

### Flash

1. Open [firmware/g500_nano/g500_nano.ino](firmware/g500_nano/g500_nano.ino) in Arduino IDE
2. Select board: `Arduino Nano`
3. Select the correct COM/serial port
4. Click **Upload**

The Nano will print `{"status":"ready"}` over serial when it boots successfully.

### Serial protocol

The server sends newline-terminated JSON at 115200 baud:
```json
{"t": 0.5, "s": -0.3}
```
- `t` — throttle: `-1.0` (full reverse) → `0` (stop) → `1.0` (full forward)
- `s` — steering: `-1.0` (full left) → `0` (straight) → `1.0` (full right)

**Failsafe:** if no command is received for 500 ms, the Arduino stops the car automatically.

---

## Repository Structure

```
g500_rc_car/
├── README.md                  ← you are here
├── CLAUDE.md                  ← AI working guide + architecture notes
├── package.json               ← npm workspace root
├── .gitignore
│
├── server/                    ← Node.js backend (runs on RPi)
│   ├── package.json
│   └── index.js               — Express + WebSocket + serial bridge
│
├── ui/                        ← Phone FPV controller (plain HTML)
│   ├── package.json
│   └── index.html             — touch joystick + throttle + camera feed
│
├── camera/                    ← Python camera service (runs on RPi)
│   ├── requirements.txt
│   └── stream.py              — OAK-D Lite → MJPEG HTTP stream
│
├── firmware/                  ← Arduino Nano code
│   └── g500_nano/
│       └── g500_nano.ino
│
├── docker/                    ← Docker deployment
│   ├── Dockerfile.server
│   ├── Dockerfile.camera
│   └── docker-compose.yml
│
└── docs/                      ← Per-topic progress tracking
    ├── docker-setup.md
    ├── raspberry-pi-setup.md
    ├── arduino-firmware.md
    ├── camera-streaming.md
    ├── serial-communication.md
    └── web-ui.md
```

---

## npm Scripts Reference

Run from the project root:

| Command | Description |
|---------|-------------|
| `npm install` | Install all dependencies (server + ui) |
| `npm run dev` | Start server with hot-reload (nodemon) |
| `npm start` | Start server in production mode |
| `npm run build` | No-op (UI has no build step) |

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | HTTP + WebSocket port |
| `SERIAL_PATH` | auto-detect | Arduino serial port path (e.g. `/dev/ttyUSB0`) |
| `SERIAL_BAUD` | `115200` | Serial baud rate |
| `CAMERA_URL` | `http://localhost:8080/stream` | Internal camera service URL |

Set these in a `.env` file in the project root (never commit this file).

---

## Ports

| Port | Service |
|------|---------|
| `3000` | Main server — UI, WebSocket, camera proxy |
| `8080` | Camera MJPEG stream (internal; accessed via `:3000/stream`) |

---

## Troubleshooting

**No serial connection to Arduino**
→ Check `ls /dev/ttyUSB* /dev/ttyACM*` and set `SERIAL_PATH` accordingly.
→ Make sure your user is in the `dialout` group: `sudo usermod -aG dialout $USER`.

**Camera feed not showing**
→ Confirm the OAK-D Lite is on a USB 3.0 port (blue port on RPi 5).
→ Check camera service logs: `docker compose logs camera`.
→ Try accessing `http://<rpi-ip>:8080/stream` directly.

**UI not loading on phone**
→ Confirm phone and RPi are on the same WiFi.
→ Check server is running: `docker compose ps` or `curl http://<rpi-ip>:3000/health`.

**Car doesn't respond to controls**
→ Check WebSocket connection status indicator at the top of the UI.
→ Check server logs: `docker compose logs server`.
→ Verify Arduino is flashed and serial cable is connected.

---

## Contributing / Development Notes

- Before working on any subsystem, read its `docs/[topic].md` file first.
- After any attempt (success or failure), update the relevant doc file.
- See [CLAUDE.md](CLAUDE.md) for full working conventions.
