# Topic: Camera Streaming (U20CAM 1080p → Electron / Browser)

**Status: `[x] Scaffolded — pending first live run test`**

---

## Problem

Stream live video from the Innomaker U20CAM 1080p USB webcam on the RC car to the Electron
desktop UI (and optionally a phone browser) with low enough latency for FPV driving.
Target: < 200ms glass-to-glass latency.

---

## Theory

### Camera Hardware

**Innomaker U20CAM 1080p**
- Interface: USB 2.0, UVC (USB Video Class) — plug-and-play, no proprietary SDK needed
- V4L2 device: `/dev/video0` (confirmed on RPi 5)
- USB ID: `0c45:6366 Microdia Webcam Vitade AF`
- Resolution: up to 1280×720 @ 30 fps (tested), supports 1920×1080 @ lower fps
- Driver: built into Linux kernel (uvcvideo)

### Streaming Options

| Method | Latency | Browser Support | Complexity |
|--------|---------|-----------------|------------|
| **MJPEG over HTTP** | ~150–300ms | Universal (`<img>` tag) | Very low |
| **HLS (FFmpeg)** | 2–10s | Universal | Low |
| **WebRTC** | ~50–100ms | Universal modern | High |
| **WebSockets + JPEG frames** | ~100–200ms | Universal | Medium |

### Decision
- **Phase 1:** MJPEG over HTTP — simplest, works in any browser or Electron `<img>` tag
- **Phase 2:** WebRTC — only if MJPEG latency is unacceptable for real driving

### MJPEG Pipeline
```
Innomaker U20CAM 1080p  (/dev/video0)
    │ USB 2.0, V4L2
    ▼
camera/webcam_stream.py
    ├── cv2.VideoCapture(0, cv2.CAP_V4L2)
    ├── capture thread → JPEG encode (quality 80)
    └── shared _frame bytes (thread-safe lock)
          │
          ▼
Flask HTTP server  (port 8080)
    └── GET /stream  multipart/x-mixed-replace; boundary=frame
          │
          ├── Electron BrowserWindow (localhost only)
          │     <img src="http://127.0.0.1:8080/stream">
          │
          └── Phone browser (same WiFi)
                <img src="http://<rpi-ip>:8080/stream">
```

### Environment Variables
```
STREAM_HOST=0.0.0.0   # bind address (default)
STREAM_PORT=8080       # HTTP port (default)
CAMERA_INDEX=0         # V4L2 device index → /dev/video0
JPEG_QUALITY=80        # 0–100
TARGET_FPS=30
```

---

## Unit Tests

### File: `tests/camera/test_stream.py`
Mocks `cv2.VideoCapture` — no hardware needed.

**Run command:**
```bash
pytest tests/camera/test_stream.py -v
```

**Tests:**

| # | Test | Pass Condition |
|---|------|----------------|
| 1 | MJPEG endpoint returns 200 | `GET /stream` → HTTP 200 + `multipart/x-mixed-replace` content-type |
| 2 | Health endpoint | `GET /health` → `{"status":"ok","frame_ready":true}` |
| 3 | JPEG frame valid | Each frame boundary parses as a valid JPEG |
| 4 | Frame rate ≥ 20 fps | 20 frames received in ≤ 1.1 seconds (mocked) |
| 5 | Server survives camera disconnect | Camera error → service keeps running, `frame_ready: false` |

**Manual test — run server directly:**
```bash
python3 camera/webcam_stream.py
# Open in browser: http://<rpi-ip>:8080/stream
# Or launch Electron: npm run app
```

**Webcam detection check:**
```bash
lsusb                           # USB device list
v4l2-ctl --list-devices         # V4L2 device names
v4l2-ctl -d /dev/video0 --list-formats-ext   # supported resolutions
```

### Manual Latency Test
```
1. Open stream in Electron or browser
2. Hold a running clock in front of the camera
3. Screenshot or measure time between real clock and stream display
4. Target: < 200ms
```

---

## Attempts

### 2026-02-22 — Initial webcam detection and stream scaffold
**What was done:**
- Plugged in Innomaker U20CAM 1080p; detected immediately at `/dev/video0`
- `lsusb`: Bus 003 Device 002: `0c45:6366 Microdia Webcam Vitade AF`
- `v4l2-ctl --list-devices`: confirmed `Innomaker-U20CAM-1080p-S1` → `/dev/video0`, `/dev/video1`
- Created `camera/webcam_stream.py` — OpenCV capture thread + Flask MJPEG server
- Integrated into Electron app: `electron/main.js` spawns webcam_stream.py, polls `/health`, loads window
- `npm run app` wired up end-to-end

**Result:** Scaffold complete. First live run pending.

---

## Issue Log

_No issues yet._

---

## Current Status

`[x] webcam_stream.py scaffolded`
`[x] Integrated into Electron app (npm run app)`
`[ ] First live run confirmed`
`[ ] Latency measured`
`[ ] Phone browser access tested`

### Next Actions
- [ ] Run `npm run app` with webcam connected — confirm stream appears in Electron window
- [ ] Measure latency
- [ ] Test phone browser access on local WiFi: `http://<rpi-ip>:8080/stream`
- [ ] Write and run `tests/camera/test_stream.py`

---

## Solution

**Camera:** Innomaker U20CAM 1080p, `/dev/video0`, V4L2/UVC.
**Server:** `camera/webcam_stream.py` — OpenCV + Flask, MJPEG on port 8080.
**Client:** Electron `<img>` tag or any browser pointing to `/stream`.
**Launch:** `npm run app` (Electron handles starting the server automatically).
