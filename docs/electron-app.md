# Topic: Electron Desktop App (FPV Driving UI)

**Status: `[x] Scaffolded — blocked on display (RPi is headless, need X11 forwarding)`**

---

## Problem

Provide a native desktop FPV UI on any laptop — `npm run app` on any machine and it shows
the live camera feed from the RPi, with no per-machine setup beyond `npm install`.

---

## Theory

### Core Architectural Rule: Electron Is a Dumb Pass-Through

**This is permanent. Do not add backend logic to Electron.**

The Electron app is a zero-logic display shell. Its entire job:
1. Read `CAMERA_URL` env var (default: `http://g500.local:8080`)
2. Open a `BrowserWindow`
3. Pass the URL to the renderer via IPC
4. The renderer renders an `<img src="...">` pointing at the MJPEG stream

That is all. Electron knows nothing about:
- How the camera works
- Where the RPi is or what it runs
- Serial ports, Arduino, motors, ROS — nothing

**The RPi is the single source of truth for all hardware and streaming.**
Electron is interchangeable with a browser tab — `http://g500.local:8080/stream` in any
browser does the same thing without Electron at all.

**Why this matters:**
- Any laptop works with zero per-machine setup (just `npm install` once)
- No Python, no OpenCV, no Flask, no hardware drivers needed on the laptop
- If you switch laptops, the new one just needs `git clone` + `npm install`
- If Electron breaks, open a browser tab — nothing is lost

**What MUST live on the RPi (not Electron):**
| Service | File | Auto-start |
|---------|------|------------|
| Camera stream | `camera/webcam_stream.py` | systemd `g500-camera.service` |
| WebSocket + serial bridge | `server/index.js` | systemd (planned) |
| ROS 2 nodes | `ros/g500/` | Docker (planned) |

### Architecture

```
Any laptop
    └── npm run app
            │
            ▼
    electron/main.js  (5 lines of real logic)
        ├── reads CAMERA_URL env var  →  http://g500.local:8080
        └── opens BrowserWindow
                │ preload.js passes URL via IPC
                ▼
        Renderer (index.html)
            └── <img src="http://g500.local:8080/stream">
                  │  HTTP  │
                  ▼        ▼
             RPi 5 — webcam_stream.py (systemd, always running)
                  └── OpenCV → Flask MJPEG → /dev/video1
```

### Laptop Setup (one time, any machine)
```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
npm install          # downloads Electron binary (~100 MB), only needed once
npm run app          # connect to http://g500.local:8080 by default
```

Override camera URL (if mDNS not working):
```bash
CAMERA_URL=http://192.168.1.107:8080 npm run app
```

### RPi Setup (one time, permanent)
```bash
# Install and enable the camera service (auto-starts on every boot)
sudo cp camera/g500-camera.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable g500-camera
sudo systemctl start g500-camera
```

### Security Model
- `contextIsolation: true`, `nodeIntegration: false` — renderer has no direct Node access
- `preload.js` uses `contextBridge` to expose only `window.g500.onCameraStatus()`

### UI Features (current)
- Full-window MJPEG camera feed
- Live FPS counter + resolution display
- Status dot: pulsing (connecting) → green (live) → red (error)
- Corner bracket + crosshair HUD overlays
- Auto-retry on stream error (3 s backoff)
- Drag region on top bar

---

## Unit Tests

Manual test checklist (Electron requires a display — run from a laptop, not the RPi):

**Precondition:** RPi is running (`npm run g500:start` is active).

| # | Test | Pass Condition |
|---|------|----------------|
| 1 | App launches | `npm run app` opens a window without crashing |
| 2 | Stream appears | Camera feed visible in window within 5 s |
| 3 | Status dot goes green | Dot turns green when first frame arrives |
| 4 | FPS counter updates | Bottom bar shows `N fps` (target ≥ 25) |
| 5 | Resolution shown | Bottom bar shows e.g. `640×480` |
| 6 | Stream retry on error | Stop camera on RPi → red dot + overlay → restart camera → recovers |
| 7 | URL override works | `G500_URL=http://192.168.1.107:3000 npm run app` connects using IP |

**Run command (from laptop):**
```bash
npm run app
# or with explicit IP:
G500_URL=http://192.168.1.107:3000 npm run app
```

---

## Attempts

### 2026-02-22 — Attempt 1: Initial scaffold
**What was done:**
- Created `electron/` npm workspace with `package.json`, `main.js`, `preload.js`, `index.html`
- Added `electron` workspace to root `package.json`; added `npm run app` script
- Installed Node.js v20.19.2 + npm 9.2.0 via apt (were not previously installed on RPi)
- Installed Electron v20.18.3 via `npm install --workspace=electron`
  - Note: apt/npm repos provide Electron 20 for ARM64 Linux, not Electron 33 as originally targeted
  - Electron 20 is sufficient for all current features
- `main.js`: spawns `camera/webcam_stream.py`, polls `/health` (40 retries × 500 ms = 20 s max),
  then opens `BrowserWindow`
- `index.html`: dark FPV UI, `<img id="feed">` for MJPEG, live FPS counter, status dot, HUD overlays
- `preload.js`: exposes `window.g500.onCameraStatus(cb)` via `contextBridge`

**Result:** Scaffold complete. `npm run app` command wired up. Proceeded to live test.

---

### 2026-02-22 — Attempt 2: First live run — blocked by headless RPi
**What was done:**
- Ran `npm run app` on RPi 5 via SSH (no monitor attached)

**Result:** FAIL → see ISSUE-001.

**Key insight from this failure:** Electron should never run on the RPi.
The RPi is headless. Electron is a laptop tool. This failure drove the architectural decision
to make Electron a pure remote client (dumb pass-through) that runs only on the laptop.

---

### 2026-02-22 — Attempt 3: Architecture pivot — Electron as dumb remote client
**What was done:**
- Rewrote `electron/main.js`: removed all process spawning, health checks, camera logic
- Electron now reads `G500_URL` env var (default `http://g500.local:3000`) and opens a window
- All services moved to RPi: `scripts/start.sh` starts camera + server together
- `npm run g500:start` wired up in root `package.json`
- Electron connects to port 3000 (server), which already proxies camera at `/stream`
- Single entry point: laptop only needs to know `G500_URL` (one hostname, one port)

**Result:** Architecture locked. Pending first live test from actual laptop.

---

## Issue Log

### ISSUE-001: Electron fails with "Missing X server or $DISPLAY" on headless RPi
**Date:** 2026-02-22
**Severity:** Medium (blocks Electron UI; camera service still testable independently)
**Symptom:**
```
[electron] The display compositor is frequently crashing.
Goodbye.
Error: Missing X server or $DISPLAY
```
**Expected:** Electron window opens
**Steps to reproduce:**
  1. SSH into RPi (no physical monitor attached)
  2. `npm run app`
**Diagnosed cause:**
  Electron wraps Chromium which requires a graphical display server (X11 or Wayland).
  The RPi is running headless (SSH only, no monitor, no display server).
  `$DISPLAY` is not set in the SSH session.
**Fix applied (partial):** Camera service confirmed working independently via browser.
  Full Electron test requires one of:
  - **Option A (easiest for Windows):** Install VcXsrv on the Windows laptop, then SSH with X11 forwarding:
    ```
    ssh -X yze@<rpi-ip>
    export DISPLAY=localhost:10.0
    npm run app
    ```
  - **Option B:** Attach a physical monitor to the RPi HDMI port and run with `DISPLAY=:0`
  - **Option C:** Use a virtual display: `Xvfb :1 -screen 0 1280x720x24 &` then `DISPLAY=:1 npm run app`
    (renders offscreen — useful for testing that the process launches without crashing)
**Status:** Open — pending X11 forwarding setup
**rosbag:** N/A

---

## Current Status

`[x] Scaffold complete`
`[x] Camera service confirmed working (browser stream at http://192.168.1.107:8080/stream)`
`[x] Architecture locked — Electron is a dumb pass-through, runs on laptop only`
`[x] npm run g500:start — boots camera + server on RPi`
`[x] npm run app — single command on laptop, connects to http://g500.local:3000`
`[ ] First live end-to-end test from actual laptop`
`[ ] Driving controls (joystick → WebSocket → server → Arduino) — deferred`

### Next Actions
- [ ] On RPi: `npm run g500:start` and confirm camera + server both start cleanly
- [ ] On laptop: `npm run app` and confirm FPV window shows camera feed
- [ ] Log result as Attempt #4 (pass or fail)
- [ ] Add joystick/keyboard drive controls to index.html (WebSocket to `wsUrl`)

---

## Solution

### The complete workflow

**On the RPi — every session:**
```bash
ssh yze@g500.local
cd ~/g500_rc_car
npm run g500:start
```
Starts camera (`:8080`) + server (`:3000`). Leave this running.

**On any laptop — every session:**
```bash
npm run app
```
Opens the FPV Electron window. Done.

**First-time laptop setup (once ever):**
```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
npm install
npm run app
```

**If mDNS (`g500.local`) doesn't work:**
```bash
G500_URL=http://192.168.1.107:3000 npm run app
```

---

**Key files:**

| File | Role |
|------|------|
| `scripts/start.sh` | RPi boot script — starts camera + server, Ctrl+C kills both |
| `electron/main.js` | Laptop — opens window, reads `G500_URL`, passes URLs to renderer. NO other logic. |
| `electron/preload.js` | IPC bridge — exposes `window.g500.onReady()` to renderer |
| `electron/index.html` | Renderer — MJPEG `<img>` tag, FPS counter, status dot, HUD overlays |
| `camera/webcam_stream.py` | Camera server — runs on RPi only, managed by `g500:start` |
| `server/index.js` | Node.js server — WebSocket + serial bridge + proxies camera at `/stream` |

**`G500_URL` env var (laptop only):**
```
Default:  http://g500.local:3000     (mDNS — works when RPi hostname = g500.local)
Override: G500_URL=http://192.168.1.107:3000 npm run app
```

**What Electron does NOT do (permanent rule — see CLAUDE.md Rule #7):**
- Does NOT spawn any processes
- Does NOT talk to hardware
- Does NOT contain retry logic or state machines
- Does NOT know about Python, OpenCV, serial, Arduino, or ROS
- Does NOT run on the RPi
