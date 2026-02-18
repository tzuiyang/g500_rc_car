# Topic: Docker / npm Environment Setup

**Status: `[ ] In Progress`**

---

## Problem

The project must be portable — runnable on any machine (Windows dev laptop, Mac, or the Raspberry Pi 5 itself) without manual dependency hell. A single command should bring up the full system.

---

## Theory

### Options Considered

| Option | Pros | Cons |
|--------|------|------|
| **Docker Compose** | Fully isolated, reproducible, cross-platform | USB device passthrough (camera, Arduino serial) needs extra config on Linux/RPi |
| **npm workspaces** | Simple for JS-only parts (UI, server) | Doesn't isolate OS-level deps (Python, DepthAI, ffmpeg) |
| **Combined: Docker for runtime, npm for dev** | Best of both | Slightly more complex to set up |

### Decision
Use **Docker Compose** as the primary runtime target (for RPi deployment), and **npm scripts** for developer tooling on any workstation. The two are complementary:
- `npm run dev` — local development (UI hot-reload, mock serial)
- `docker compose up` — full production stack on RPi

### Docker Architecture Plan
```
docker-compose.yml
├── service: server       (Node.js — WebSocket + HTTP API)
├── service: ui           (Nginx serving built React/HTML UI)
└── service: camera       (Python + DepthAI — OAK-D Lite stream)
```

USB passthrough in Docker on Linux (RPi):
- Arduino serial: `/dev/ttyUSB0` or `/dev/ttyACM0` → volume mount in server container
- OAK-D Lite: USB device passthrough via `privileged: true` or `devices:` in compose

---

## Attempts

### Attempt 1 — Initial scaffold (2026-02-18)
**What we did:** Created project directory structure and this documentation.
**Result:** Documentation only — no code yet.

### Attempt 2 — Full scaffold created (2026-02-18)
**What we did:** Created all environment files:
- `package.json` — npm workspace root (`server` + `ui`)
- `server/package.json` + `server/index.js` — Express + WebSocket + serialport
- `ui/package.json` + `ui/index.html` — plain HTML FPV controller (no build step)
- `camera/stream.py` + `camera/requirements.txt` — DepthAI MJPEG stream via Flask
- `docker/Dockerfile.server` — Node.js 20 slim
- `docker/Dockerfile.camera` — Python 3.11 + DepthAI + Flask
- `docker/docker-compose.yml` — orchestrates server + camera; USB/serial passthrough configured

**Result:** All files created. Not yet tested — awaiting RPi hardware.

**Known gotchas to watch:**
- `devices:` in compose uses `${SERIAL_PATH:-/dev/ttyUSB0}`. If Nano appears as `/dev/ttyACM0`, set `SERIAL_PATH=/dev/ttyACM0` in a `.env` file.
- OAK-D Lite requires `privileged: true` in camera container — standard DepthAI Docker requirement.
- Arduino firmware requires **ArduinoJson** library installed via Arduino Library Manager before flashing.
- The UI docker-compose plan changed: UI is served directly by the Node.js Express server (no separate Nginx container needed for v1).

---

## Current Status

`[ ] In Progress` — Scaffold complete. Awaiting hardware test on RPi 5.

### Next Actions
- [x] Create root `package.json` with workspace scripts
- [x] Create `server/package.json` + `server/index.js`
- [x] Create `ui/package.json` + `ui/index.html`
- [x] Write `docker/Dockerfile.server`
- [x] Write `docker/Dockerfile.camera`
- [x] Write `docker/docker-compose.yml`
- [ ] Run `npm install` locally to verify package resolution
- [ ] Test server-only: `docker compose up server` on dev machine (no hardware)
- [ ] Test on RPi 5: `docker compose up` with Arduino serial passthrough
- [ ] Test OAK-D Lite USB passthrough in camera container on RPi

---

## Solution

_Not yet solved — awaiting hardware test._
