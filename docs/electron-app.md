# Topic: Electron Desktop App (FPV Driving UI)

**Status: `[x] WORKING — end-to-end confirmed (Electron on laptop → RPi IP → live stream)`**

---

## Problem

Provide a native desktop FPV UI on any laptop — `npm run app` on any machine shows the live
camera feed from the RPi, with no per-machine setup beyond `npm install`.

---

## Theory

### Core Architectural Rule: Electron Is a Dumb Pass-Through

**This is permanent. Do not add backend logic to Electron.**

The Electron app is a zero-logic display shell. Its entire job:
1. Read `G500_URL` env var, or `rpiUrl` from `g500.config.json` (default: `http://192.168.1.107:3000`)
2. Open a `BrowserWindow`
3. Pass `streamUrl` and `wsUrl` to the renderer via IPC
4. The renderer renders `<img src="${streamUrl}">` pointing at the MJPEG stream

That is all. Electron knows nothing about:
- How the camera works
- Where the RPi is or what it runs
- Serial ports, Arduino, motors, ROS — nothing

**The RPi is the single source of truth for all hardware and streaming.**
Electron is interchangeable with a browser tab — opening `http://192.168.1.107:8080/stream`
in any browser does the same thing without Electron at all.

**Why this matters:**
- Any laptop works with zero per-machine setup (just `npm install` once)
- No Python, no OpenCV, no Flask, no hardware drivers needed on the laptop
- If you switch laptops, the new one just needs `git clone` + `npm install`
- If Electron breaks, open a browser tab — nothing is lost

**What MUST live on the RPi (not Electron):**
| Service | File | Started by |
|---------|------|-----------|
| Camera stream | `camera/webcam_stream.py` | `npm run g500:start` |
| WebSocket + serial bridge | `server/index.js` | `npm run g500:start` |
| ROS 2 nodes | `ros/g500/` | Docker (planned) |

### Architecture

```
Any laptop
    └── npm run app
            │
            ▼
    electron/main.js
        ├── reads g500.config.json → rpiUrl = http://192.168.1.107:3000
        │   (G500_URL env var overrides when set)
        └── opens BrowserWindow
                │ preload.js passes streamUrl + wsUrl via IPC
                ▼
        Renderer (index.html)
            └── <img src="http://192.168.1.107:3000/stream">
                  │  HTTP (MJPEG)
                  ▼
             RPi 5 — server/index.js :3000
                  └── proxies camera/webcam_stream.py :8080
                            └── OpenCV → Flask MJPEG → /dev/video1
```

### Configuration: g500.config.json

The RPi IP is stored in `g500.config.json` at the repo root:
```json
{ "rpiUrl": "http://192.168.1.107:3000" }
```

- **Edit this file when the RPi IP changes** (DHCP lease, new network, etc.) — no code changes needed
- `electron/main.js` reads it with `require('../g500.config.json')`
- `G500_URL` env var overrides the config file: `G500_URL=http://other-ip:3000 npm run app`

### Why Not mDNS (g500.local)?

`g500.local` requires Bonjour/Avahi to resolve the hostname. On Windows this requires
Apple's Bonjour Print Services driver, which is not installed by default. Without it,
Windows returns "host not found" and Electron shows "Stream unreachable".
The IP address `192.168.1.107` works on any OS without any additional software.

### Laptop Setup (one time, any machine)
```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
npm install          # downloads Electron binary (~100 MB), only needed once
npm run app          # connects to IP in g500.config.json
```

Override IP without editing config:
```bash
G500_URL=http://192.168.1.107:3000 npm run app
```

### RPi Setup (every session)
```bash
ssh yze@192.168.1.107
cd ~/g500_rc_car
npm run g500:start
# — starts camera on :8080 and server on :3000 — leave this running
```

### Security Model
- `contextIsolation: true`, `nodeIntegration: false` — renderer has no direct Node access
- `preload.js` uses `contextBridge` to expose only `window.g500.onReady()`

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

**Precondition:** RPi is running (`npm run g500:start` is active — camera + server both up).

| # | Test | Pass Condition |
|---|------|----------------|
| 1 | App launches | `npm run app` opens a window without crashing |
| 2 | Stream appears | Camera feed visible in window within 5 s |
| 3 | Status dot goes green | Dot turns green when first frame arrives |
| 4 | FPS counter updates | Bottom bar shows `N fps` (target ≥ 20) |
| 5 | Resolution shown | Bottom bar shows e.g. `640×480` |
| 6 | Stream retry on error | Stop camera on RPi → red dot + overlay → restart camera → recovers |
| 7 | URL override works | `G500_URL=http://192.168.1.107:3000 npm run app` connects using IP |
| 8 | Config file read | Changing `rpiUrl` in `g500.config.json` and restarting connects to new IP |

**Run command (from laptop):**
```bash
npm run app
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
- `main.js`: spawned `camera/webcam_stream.py`, polled `/health` (40 retries × 500 ms = 20 s max),
  then opened `BrowserWindow`
- `index.html`: dark FPV UI, `<img id="feed">` for MJPEG, live FPS counter, status dot, HUD overlays
- `preload.js`: exposed `window.g500.onCameraStatus(cb)` via `contextBridge`

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
- Single entry point: laptop only needs to know one hostname + port

**Result:** Architecture locked. Pending first live test from actual laptop.

---

### 2026-02-22 — Attempt 4: First live end-to-end test — mDNS failure + IP fix
**What was done (step by step):**

**Step 1 — RPi side: `npm run g500:start`**
- Camera started successfully (`[camera] Opened /dev/video1 at 640×480 @ 25fps`)
- Server failed immediately with:
  ```
  Error: listen EADDRINUSE: address already in use :::3000
  errno: -98, syscall: 'listen', address: '0.0.0.0', port: 3000
  ```
  → see ISSUE-002

**Step 2 — Diagnose port conflict:**
- Ran `lsof -i :3000` → no output (port was already freed by the time we checked — server had crashed)
- Root cause: a previous `node server/index.js` process from an earlier manual test had occupied port 3000,
  then crashed leaving the port briefly held, then released
- `start.sh` was missing cleanup of stale processes before starting

**Step 3 — Fix `scripts/start.sh`:**
- Added stale-process cleanup block at the top:
  ```bash
  pkill -f "webcam_stream.py" 2>/dev/null || true
  fuser -k 3000/tcp 2>/dev/null || true
  fuser -k 8080/tcp 2>/dev/null || true
  sleep 0.5  # give ports time to release
  ```
- Added `CAMERA_PID` and `SERVER_PID` tracking for cleaner `wait` call
- Removed `set -e` (would abort on non-zero pkill/fuser exits which are expected on first clean boot)

**Step 4 — RPi side: retry `npm run g500:start`:**
- Both camera and server started cleanly
- Confirmed accessible from browser: `http://192.168.1.107:3000/stream` ✓

**Step 5 — Laptop side: `npm run app`:**
- Electron window opened
- Screen showed: **"Stream unreachable: http://g500.local:3000/stream"**
- `g500.local` mDNS hostname not resolving on Windows laptop → see ISSUE-003

**Step 6 — Fix mDNS issue with g500.config.json:**
- Created `g500.config.json` at repo root: `{ "rpiUrl": "http://192.168.1.107:3000" }`
- Updated `electron/main.js`:
  ```javascript
  const config = require('../g500.config.json')
  const BASE = (process.env.G500_URL || config.rpiUrl).replace(/\/$/, '')
  ```
- `G500_URL` env var still overrides if set
- Committed + pushed + `git pull` on laptop

**Step 7 — Laptop side: retry `npm run app`:**
- Electron connected to `http://192.168.1.107:3000`
- **Live stream appeared in window** ✓

**Result:** PASS — end-to-end confirmed working.

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
**Fix applied:**
  Architectural decision: Electron never runs on the RPi. The RPi is headless.
  Electron runs on the laptop only. `npm run app` on laptop, `npm run g500:start` on RPi.
  This drove the entire dumb-pass-through architecture.
**Status:** Fixed (by design — Electron moved to laptop-only)
**rosbag:** N/A

---

### ISSUE-002: Port 3000 EADDRINUSE on server start (errno: -98)
**Date:** 2026-02-22
**Severity:** High
**Symptom:**
```
Error: listen EADDRINUSE: address already in use :::3000
  errno: -98, syscall: 'listen', address: '0.0.0.0', port: 3000
```
Camera started fine, server crashed immediately.
**Expected:** Server binds to port 3000 and starts normally
**Steps to reproduce:**
  1. Run `npm run g500:start` once (or manually run `node server/index.js`)
  2. Kill it (Ctrl+C or crash)
  3. Run `npm run g500:start` again immediately
**Diagnosed cause:**
  A previous `node server/index.js` process (from an earlier manual test session) had the port.
  `scripts/start.sh` had no stale-process cleanup — it just started new processes.
  On Linux, `errno -98` = `EADDRINUSE` (the POSIX address-in-use error, different from macOS which is -48).
**Fix applied:**
  Added stale-process cleanup to `scripts/start.sh` before starting any service:
  ```bash
  pkill -f "webcam_stream.py" 2>/dev/null || true
  fuser -k 3000/tcp 2>/dev/null || true
  fuser -k 8080/tcp 2>/dev/null || true
  sleep 0.5   # give OS time to release ports
  ```
**Status:** Fixed
**rosbag:** N/A

---

### ISSUE-003: mDNS hostname g500.local not resolving on Windows laptop
**Date:** 2026-02-22
**Severity:** High (blocks Electron from connecting to RPi — stream shows "unreachable")
**Symptom:**
  Electron window shows:
  ```
  Stream unreachable
  http://g500.local:3000/stream
  ```
  RPi services are running and confirmed accessible via IP address.
**Expected:** `g500.local` resolves to RPi IP and stream loads in Electron
**Steps to reproduce:**
  1. RPi running `npm run g500:start` (confirmed accessible via IP)
  2. Windows laptop: `npm run app`
  3. Electron window shows "Stream unreachable: http://g500.local:3000/stream"
**Diagnosed cause:**
  mDNS (`.local` hostname resolution) requires Bonjour on Windows (Apple's Bonjour Print Services).
  Not installed by default. Without it, Windows DNS lookup for `g500.local` fails silently.
  The RPi uses `avahi-daemon` to advertise `g500.local` via mDNS, but the Windows side has no mDNS
  resolver to receive it. Linux and macOS have mDNS built in; Windows does not.
**Fix applied:**
  - Created `g500.config.json` at repo root with the RPi's IP address:
    ```json
    { "rpiUrl": "http://192.168.1.107:3000" }
    ```
  - Updated `electron/main.js` to read from config file instead of hardcoding `g500.local`:
    ```javascript
    const config = require('../g500.config.json')
    const BASE = (process.env.G500_URL || config.rpiUrl).replace(/\/$/, '')
    ```
  - To change the IP: edit `g500.config.json` only — no code changes needed
  - `G500_URL` env var still works as an override
**Status:** Fixed
**rosbag:** N/A

---

## Current Status

`[x] Scaffold complete`
`[x] Camera service confirmed working (browser stream at http://192.168.1.107:8080/stream)`
`[x] Architecture locked — Electron is a dumb pass-through, runs on laptop only`
`[x] npm run g500:start — starts camera (:8080) + server (:3000) on RPi with stale-process cleanup`
`[x] npm run app — single command on laptop, connects to IP from g500.config.json`
`[x] ISSUE-001 (headless) — resolved by design (Electron on laptop only)`
`[x] ISSUE-002 (EADDRINUSE) — fixed in scripts/start.sh with stale-process cleanup`
`[x] ISSUE-003 (mDNS Windows) — fixed with g500.config.json IP-based config`
`[x] First live end-to-end test from actual laptop — PASS`
`[ ] Driving controls (joystick → WebSocket → server → Arduino) — deferred`

### Next Actions
- [ ] Measure stream latency (clock-in-front-of-camera method)
- [ ] Add joystick/keyboard drive controls to `electron/index.html` (WebSocket to `wsUrl`)
- [ ] Test auto-recovery: unplug camera → replug → confirm stream resumes without restarting

---

## Solution

### The complete workflow

**On the RPi — every session:**
```bash
ssh yze@192.168.1.107      # or use g500.local if on a Linux/macOS machine
cd ~/g500_rc_car
npm run g500:start
```
Starts camera (`:8080`) + server (`:3000`). Leave this running.

**On any laptop — every session:**
```bash
npm run app
```
Opens the FPV Electron window, connects to `http://192.168.1.107:3000`. Done.

**First-time laptop setup (once ever):**
```bash
git clone https://github.com/tzuiyang/g500_rc_car
cd g500_rc_car
npm install
npm run app
```

**If the RPi IP changes:**
```bash
# Edit g500.config.json at the repo root:
{ "rpiUrl": "http://<new-ip>:3000" }
# then git commit + push + pull on all laptops
```

---

**Key files:**

| File | Role |
|------|------|
| `g500.config.json` | RPi IP config — edit here when IP changes |
| `scripts/start.sh` | RPi boot script — starts camera + server, cleans stale processes, Ctrl+C kills both |
| `electron/main.js` | Laptop — opens window, reads `g500.config.json` (or `G500_URL` env var), passes URLs to renderer. NO other logic. |
| `electron/preload.js` | IPC bridge — exposes `window.g500.onReady()` to renderer |
| `electron/index.html` | Renderer — MJPEG `<img>` tag, FPS counter, status dot, HUD overlays |
| `camera/webcam_stream.py` | Camera server — runs on RPi only, managed by `g500:start` |
| `server/index.js` | Node.js server — WebSocket + serial bridge + proxies camera at `/stream` |

**What Electron does NOT do (permanent rule — see CLAUDE.md Rule #7):**
- Does NOT spawn any processes
- Does NOT talk to hardware
- Does NOT contain retry logic or state machines
- Does NOT know about Python, OpenCV, serial, Arduino, or ROS
- Does NOT run on the RPi
