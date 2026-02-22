# Topic: Electron Desktop App (FPV Driving UI)

**Status: `[x] Scaffolded — pending first live run`**

---

## Problem

Provide a native desktop UI on the Raspberry Pi (or any connected display / SSH-forwarded display)
for FPV driving. The UI must show the live camera stream and be launchable with a single command.

---

## Theory

### Why Electron?
The project originally targeted a phone browser for the UI. An Electron app adds a second
display path that is useful when:
- A monitor is attached to the RPi (e.g. during bench testing)
- You want a richer native-feel UI without fighting browser security policies (CSP, mixed content)
- A desktop machine (laptop) is SSHed into the RPi and wants a local window

Electron wraps Chromium + Node.js. It can:
- Spawn the camera Python server as a child process (no manual startup needed)
- Render MJPEG streams natively via `<img>` tag (same as browser, zero extra deps)
- Use IPC to pass config from main → renderer securely

### Architecture

```
npm run app
    │
    ▼
electron/main.js  (Node.js main process)
    ├── spawn python3 camera/webcam_stream.py  → localhost:8080
    ├── poll /health until camera is ready (up to 20 s)
    └── open BrowserWindow
            │ loads electron/index.html
            │ preload.js exposes window.g500.onCameraStatus()
            ▼
        Renderer process
            └── <img src="http://127.0.0.1:8080/stream">
                 MJPEG frames update the img tag at ~30 fps
```

### Security Model
- `contextIsolation: true`, `nodeIntegration: false` — renderer has no direct Node access
- `preload.js` uses `contextBridge` to expose only `window.g500.onCameraStatus()`
- Camera stream is localhost-only (`127.0.0.1:8080`) — not exposed to network from Electron side

### UI Features (current)
- Full-window MJPEG camera feed
- Live FPS counter + resolution display
- Status dot: pulsing (connecting) → green (live) → red (error)
- Corner bracket + crosshair HUD overlays
- Auto-retry on stream error (3 s backoff)
- Drag region on top bar

---

## Unit Tests

Manual test checklist (no hardware-free automated tests yet — Electron requires a display):

| # | Test | Pass Condition |
|---|------|----------------|
| 1 | App launches | `npm run app` opens a window without crashing |
| 2 | Camera server starts | Console shows `[camera] Opened /dev/video0` |
| 3 | Stream appears | Camera feed visible in window within 5 s |
| 4 | Status dot goes green | Dot turns green when first frame arrives |
| 5 | FPS counter updates | Bottom bar shows `N fps` (target ≥ 25) |
| 6 | Resolution shown | Bottom bar shows e.g. `1280×720` |
| 7 | Window close kills camera | Python process exits when window is closed |
| 8 | Stream retry on error | Unplug camera → red dot + overlay → replug → recovers |

**Run command:**
```bash
npm run app
```

---

## Attempts

### 2026-02-22 — Initial scaffold
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

**Result:** Scaffold complete. `npm run app` command wired up. Pending first live run.

---

## Issue Log

_No issues yet._

---

## Current Status

`[x] Scaffold complete`
`[ ] First live run test`
`[ ] Driving controls (joystick / keyboard) — deferred`
`[ ] WebSocket connection to server — deferred`

### Next Actions
- [ ] Run `npm run app` with webcam connected — confirm stream appears
- [ ] Log result as Attempt #2 (pass or fail)
- [ ] If display unavailable on RPi, test with `DISPLAY=:0` or X11 forwarding
- [ ] Add driving controls (joystick input → WebSocket → server)

---

## Solution

**Launch:**
```bash
npm run app
```

**Key files:**

| File | Role |
|------|------|
| `electron/main.js` | Main process: spawns camera, opens window |
| `electron/preload.js` | IPC bridge: exposes `window.g500` API to renderer |
| `electron/index.html` | Renderer: FPV UI with MJPEG feed |
| `camera/webcam_stream.py` | Camera server: OpenCV + Flask, spawned by main.js |

**Environment variables (passed to camera server):**
```
STREAM_PORT=8080     (default)
STREAM_HOST=127.0.0.1
CAMERA_INDEX=0       (change if webcam is not /dev/video0)
```
