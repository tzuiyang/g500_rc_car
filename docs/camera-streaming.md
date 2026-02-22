# Topic: Camera Streaming (U20CAM 1080p → Electron / Browser)

**Status: `[x] WORKING — browser stream confirmed`**

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
- V4L2 device: `/dev/video1` on this RPi 5 (NOT `/dev/video0` — see ISSUE-003)
- USB ID: `0c45:6366 Microdia Webcam Vitade AF`
- Resolution: up to 1280×720 @ 30 fps (tested), supports 1920×1080 @ lower fps
- Driver: built into Linux kernel (uvcvideo)

**RPi 5 device layout:**
```
/dev/video1   ← U20CAM capture node  (use this)
/dev/video2   ← U20CAM metadata node
/dev/video19  ← RPi internal ISP/codec nodes (ignore)
...
/dev/video35  ← RPi internal ISP/codec nodes (ignore)
```
The RPi 5's internal camera ISP/codec nodes take up video19–35 (and sometimes other low numbers).
The first available USB webcam typically lands at video1 or video2, not video0.

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
Innomaker U20CAM 1080p  (/dev/video1)
    │ USB 2.0, V4L2 / UVC
    ▼
camera/webcam_stream.py
    ├── cv2.VideoCapture(1)          ← auto-detect backend (no cv2.CAP_V4L2 forced)
    ├── capture thread → JPEG encode (quality 70, 640×480)
    ├── threading.Event signals new frame
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

### Design Decisions (from debugging)

**Why no `cv2.CAP_V4L2` backend forced?**
On this RPi 5 with OpenCV 4.13, specifying `cv2.CAP_V4L2` explicitly causes the backend to reject
the camera with "can't be used to capture by index" (integer) and "can't be used to capture by name"
(path string). Auto-detection (`cv2.VideoCapture(index)` with no backend arg) works correctly.

**Why `OPENCV_VIDEOIO_PRIORITY_OBSENSOR=0`?**
Without this, OpenCV probes every V4L2 device through its Orbbec depth-camera plugin and prints
`obsensor_uvc_stream_channel.cpp:163 getStreamChannelGroup Camera index out of range` errors.
Setting this env var before `import cv2` suppresses that noise without breaking capture.

**Why `threading.Event` instead of `time.sleep`?**
Original sleep-based generator sent the same frame multiple times (visible as lag). Event-driven
approach only wakes generators when a truly new frame is ready. Combined with `CAP_PROP_BUFFERSIZE=1`,
eliminates the buffer-buildup lag.

**Why `_open_camera()` releases and recreates on failure?**
`cap.open()` on an existing broken object is unreliable. Releasing and creating a fresh
`VideoCapture` on failure is more robust and avoids leaving the USB device in a bad state.

### Environment Variables
```
STREAM_HOST=0.0.0.0    # bind address (default)
STREAM_PORT=8080        # HTTP port (default)
CAMERA_INDEX=1          # device index → /dev/video1 on this RPi 5
JPEG_QUALITY=70         # 0–100
TARGET_FPS=25
STREAM_WIDTH=640
STREAM_HEIGHT=480
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
# Health check:    http://<rpi-ip>:8080/health
```

**Webcam detection check:**
```bash
lsusb                                        # confirm 0c45:6366 present
v4l2-ctl --list-devices                      # shows which /dev/videoN the webcam is on
ls -la /dev/video*                           # see all video devices + timestamps
fuser /dev/video1                            # check if something else has the device open
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

### 2026-02-22 — Attempt 1: Initial webcam detection and stream scaffold
**What was done:**
- Plugged in Innomaker U20CAM 1080p; detected immediately
- `lsusb`: Bus 003 Device 002: `0c45:6366 Microdia Webcam Vitade AF`
- `v4l2-ctl --list-devices`: confirmed `Innomaker-U20CAM-1080p-S1` → `/dev/video0`, `/dev/video1`
- Created `camera/webcam_stream.py` — OpenCV capture thread + Flask MJPEG server
  - Original: `cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_V4L2)`, sleep-based generator, 1280×720, quality 80
- Integrated into Electron app: `electron/main.js` spawns webcam_stream.py, polls `/health`, loads window

**Result:** Scaffold complete. Proceeded to live test.

---

### 2026-02-22 — Attempt 2: First live browser test — stream worked then lagged
**What was done:**
- Ran `python3 camera/webcam_stream.py`
- Opened `http://192.168.1.107:8080/stream` in browser
- Stream showed video briefly, then froze / lagged out

**Result:** FAIL → see ISSUE-001. Applied lag fix, moved to Attempt 3.

---

### 2026-02-22 — Attempt 3: Lag fix — event-driven + buffer size
**What was done:**
- Set `cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)` — minimize OpenCV internal frame buffer
- Replaced sleep-based generator with `threading.Event`-based delivery
- Lowered defaults: 640×480 resolution, quality 70, 25fps
- Added env vars: `STREAM_WIDTH`, `STREAM_HEIGHT`

**Result:** Proceeded to test but hit new error → see ISSUE-002.

---

### 2026-02-22 — Attempt 4: Fix obsensor error — OPENCV_VIDEOIO_PRIORITY_OBSENSOR=0
**What was done:**
- Added `os.environ.setdefault('OPENCV_VIDEOIO_PRIORITY_OBSENSOR', '0')` before `import cv2`
- Moved camera open into `_open_camera()` helper (always uses `cv2.CAP_V4L2`)
- Improved retry: release + recreate VideoCapture on 5 consecutive failures (not just `cap.open()`)

**Result:** FAIL — obsensor error suppressed but V4L2 now rejected integer index → see ISSUE-003.

---

### 2026-02-22 — Attempt 5: Fix V4L2 index rejection — use device path string
**What was done:**
- Changed `cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_V4L2)` to `cv2.VideoCapture(f'/dev/video{CAMERA_INDEX}', cv2.CAP_V4L2)`

**Result:** FAIL — V4L2 also rejected path string ("can't be used to capture by name") → see ISSUE-004.
Also exposed ISSUE-005 (NameError).

---

### 2026-02-22 — Attempt 6: Drop forced V4L2 backend — let OpenCV auto-detect
**What was done:**
- Changed to `cv2.VideoCapture(CAMERA_INDEX)` with no backend argument
- Fixed NameError by removing the `device` variable reference in `capture_thread`

**Result:** FAIL — camera not at `/dev/video1` (which was CAMERA_INDEX=1 at this point) — see ISSUE-006.

Wait — actually `/dev/video0` did not exist. The camera was found at `/dev/video1`.
CAMERA_INDEX default was `0` at this point, so it tried `/dev/video0` which didn't exist.

---

### 2026-02-22 — Attempt 7: Correct CAMERA_INDEX to 1 — WORKING
**What was done:**
- Changed default `CAMERA_INDEX` from `0` to `1`
- Ran `python3 camera/webcam_stream.py`

**Result:** PASS — stream confirmed working in browser at `http://192.168.1.107:8080/stream`.

---

## Issue Log

### ISSUE-001: Stream lagged out after a few seconds
**Date:** 2026-02-22
**Severity:** High
**Symptom:** Browser stream showed video for a few seconds then froze completely
**Expected:** Continuous smooth video stream
**Steps to reproduce:**
  1. `python3 camera/webcam_stream.py`
  2. Open `http://<rpi-ip>:8080/stream` in browser
  3. Observe stream freezing after ~5–10 seconds
**Diagnosed cause:**
  Two compounding issues:
  1. OpenCV internal frame buffer (`CAP_PROP_BUFFERSIZE` not set) accumulates stale frames.
     When the MJPEG generator is slower than the capture rate, it works through a backlog of old frames.
  2. Sleep-based generator (`time.sleep(1/TARGET_FPS)`) would re-send the same frame bytes object
     if encoding hadn't produced a new frame yet — causing the consumer to receive stale data.
**Fix applied:**
  - Set `cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)` to minimize internal V4L2 buffer
  - Replaced sleep-based generator with `threading.Event`-based delivery: capture thread calls
    `_frame_event.set()` after each new frame; generator calls `_frame_event.wait(timeout=1.0)`
    and tracks `last_sent` to skip unchanged frames
  - Lowered defaults to 640×480, quality 70, 25fps to reduce bandwidth pressure
**Status:** Fixed
**rosbag:** N/A

---

### ISSUE-002: OpenCV OBSensor plugin prints errors and breaks capture
**Date:** 2026-02-22
**Severity:** High
**Symptom:**
```
[ERROR:0@193.294] global obsensor_uvc_stream_channel.cpp:163 getStreamChannelGroup Camera index out of range
[camera] Frame read failed — retrying in 1 s
```
After this, the old retry called `cap.open(CAMERA_INDEX)` (no backend arg), which re-triggered the error,
causing an infinite retry loop.
**Expected:** Clean capture without OBSensor probe errors
**Steps to reproduce:**
  1. Run `python3 camera/webcam_stream.py` with original retry logic (`cap.open(CAMERA_INDEX)`)
  2. Observe errors in console after a few seconds
**Diagnosed cause:**
  OpenCV bundles a plugin for Orbbec depth cameras (OBSensor). On startup it probes all V4L2
  devices as potential Orbbec cameras. The U20CAM is not an Orbbec camera, so the probe fails.
  The error message is cosmetic, but the subsequent `cap.open(CAMERA_INDEX)` without a backend
  argument re-probed all backends, causing the failure to cascade into capture errors.
**Fix applied:**
  - Added `os.environ.setdefault('OPENCV_VIDEOIO_PRIORITY_OBSENSOR', '0')` before `import cv2`
    to disable the OBSensor plugin before OpenCV initializes
  - Refactored retry: replaced `cap.open(CAMERA_INDEX)` with full `cap.release()` + create new
    `VideoCapture` object via `_open_camera()` helper (ensures clean state on reconnect)
**Status:** Fixed
**rosbag:** N/A

---

### ISSUE-003: V4L2 backend rejected integer index after OBSENSOR suppression
**Date:** 2026-02-22
**Severity:** High
**Symptom:**
```
[ WARN:0@82.149] global cap_v4l.cpp:914 open VIDEOIO(V4L2:/dev/video0): can't open camera by index
[ WARN:0@82.149] global cap.cpp:478 open VIDEOIO(V4L2): backend is generally available but can't be used to capture by index
[camera] ERROR: could not open /dev/video1 — retrying in 2 s
```
**Expected:** Camera opens with `cv2.VideoCapture(1, cv2.CAP_V4L2)`
**Steps to reproduce:**
  1. Set `OPENCV_VIDEOIO_PRIORITY_OBSENSOR=0`
  2. Use `cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_V4L2)`
  3. Run `python3 camera/webcam_stream.py`
**Diagnosed cause:**
  On this RPi 5 with OpenCV 4.13 + `opencv-python-headless`, explicitly requesting `cv2.CAP_V4L2`
  combined with the OBSensor env var causes V4L2 to reject integer indices. Exact root cause unclear —
  likely a version-specific interaction between the env var and backend priority resolver.
**Fix attempted:** Changed to device path string `f'/dev/video{CAMERA_INDEX}'` with `cv2.CAP_V4L2`
**Result of fix:** Still failed → see ISSUE-004
**Status:** Superseded by ISSUE-004 fix
**rosbag:** N/A

---

### ISSUE-004: V4L2 backend also rejected device path string
**Date:** 2026-02-22
**Severity:** High
**Symptom:**
```
[ WARN:0@0.137] global cap.cpp:215 open VIDEOIO(V4L2): backend is generally available but can't be used to capture by name
```
**Expected:** `cv2.VideoCapture('/dev/video1', cv2.CAP_V4L2)` opens camera
**Diagnosed cause:** Same root cause as ISSUE-003 — forcing `cv2.CAP_V4L2` is incompatible with this configuration regardless of whether an index or path is used.
**Fix applied:** Removed `cv2.CAP_V4L2` backend argument entirely.
  `cv2.VideoCapture(CAMERA_INDEX)` with auto-detection works correctly.
**Status:** Fixed
**rosbag:** N/A

---

### ISSUE-005: NameError — `device` variable not in scope in capture_thread
**Date:** 2026-02-22
**Severity:** Medium
**Symptom:**
```
NameError: name 'device' is not defined
  File "camera/webcam_stream.py", line 62, in capture_thread
    print(f'[camera] ERROR: could not open {device} — retrying in 2 s')
```
**Expected:** Error message prints the device path
**Diagnosed cause:**
  `device = f'/dev/video{CAMERA_INDEX}'` was defined inside `_open_camera()` but referenced
  in `capture_thread()` which has no access to that local variable.
**Fix applied:** Removed the `device` variable from both functions. Error message in `capture_thread`
  reconstructs the path inline: `f'/dev/video{CAMERA_INDEX}'`.
**Status:** Fixed
**rosbag:** N/A

---

### ISSUE-006: Camera not at /dev/video0 — U20CAM is at /dev/video1 on RPi 5
**Date:** 2026-02-22
**Severity:** High
**Symptom:**
```
Specified filename /dev/video0 does not exist.
[camera] ERROR: could not open /dev/video0 — retrying in 2 s
```
**Expected:** Camera opens at `/dev/video0`
**Steps to reproduce:**
  1. `ls -la /dev/video*`
  2. Observe `/dev/video0` does not exist
**Diagnosed cause:**
  The Raspberry Pi 5 reserves the low `/dev/videoN` numbers for its internal ISP and codec nodes.
  On this system `/dev/video19`–`/dev/video35` are RPi-internal.
  The U20CAM (plugged in Feb 22 at 01:59) appeared at `/dev/video1` and `/dev/video2`.
  `/dev/video1` is the capture node; `/dev/video2` is the metadata node.
  The `CAMERA_INDEX` default of `0` was wrong for this RPi 5.

  Full device layout confirmed:
  ```
  /dev/video1   crw-rw----+ ... Feb 22 01:59  ← U20CAM capture (USE THIS)
  /dev/video2   crw-rw----+ ... Feb 22 01:59  ← U20CAM metadata
  /dev/video19  crw-rw----+ ... Feb 22 00:40  ← RPi internal
  ...
  /dev/video35  crw-rw----+ ... Feb 22 00:40  ← RPi internal
  ```
**Fix applied:** Changed `CAMERA_INDEX` default from `0` to `1` in `webcam_stream.py`.
  To override: `CAMERA_INDEX=2 python3 camera/webcam_stream.py`
**Status:** Fixed
**rosbag:** N/A

---

## Current Status

`[x] webcam_stream.py — working, event-driven, robust retry`
`[x] Browser stream confirmed at http://192.168.1.107:8080/stream`
`[x] OBSensor error suppressed`
`[x] Camera correctly opens at /dev/video1`
`[x] Electron window live run confirmed — Electron runs on laptop (not RPi), connects via port 3000`
`[ ] Latency measured`
`[ ] Phone browser access tested`
`[ ] tests/camera/test_stream.py written and passing`

### Next Actions
- [ ] Test phone browser access: `http://192.168.1.107:8080/stream` from phone on same WiFi
- [ ] Measure latency (clock-in-front-of-camera method)
- [ ] Write `tests/camera/test_stream.py` with mocked VideoCapture
- [ ] Test camera auto-recovery: unplug → replug → confirm stream resumes

---

## Solution

**Status: WORKING**

**Run command:**
```bash
python3 camera/webcam_stream.py
```

**Access:**
- Browser: `http://<rpi-ip>:8080/stream`
- Health: `http://<rpi-ip>:8080/health`

**Key files:**

| File | Role |
|------|------|
| `camera/webcam_stream.py` | Camera server: OpenCV + Flask MJPEG on port 8080 |

**Final working configuration:**
```python
os.environ.setdefault('OPENCV_VIDEOIO_PRIORITY_OBSENSOR', '0')  # suppress Orbbec probe
import cv2

CAMERA_INDEX = 1          # /dev/video1 on this RPi 5
WIDTH        = 640
HEIGHT       = 480
JPEG_QUALITY = 70
TARGET_FPS   = 25

cap = cv2.VideoCapture(CAMERA_INDEX)          # no backend arg — auto-detect
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)           # minimal internal buffer → low latency
# threading.Event-based generator — only sends truly new frames
```

**Environment variables (to override defaults):**
```
CAMERA_INDEX=1          # change if webcam not at /dev/video1
STREAM_WIDTH=640
STREAM_HEIGHT=480
JPEG_QUALITY=70
TARGET_FPS=25
STREAM_PORT=8080
STREAM_HOST=0.0.0.0
```

**To find the correct CAMERA_INDEX:**
```bash
ls -la /dev/video*           # find newest timestamps = your webcam
v4l2-ctl --list-devices      # shows device names + nodes
```
