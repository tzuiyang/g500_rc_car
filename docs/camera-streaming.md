# Topic: Camera Streaming (OAK-D Lite → Phone Browser)

**Status: `[ ] Not Started`**

---

## Problem

Stream live video from the OAK-D Lite camera on the RC car to a phone browser with low enough latency for FPV driving. Target: < 200ms glass-to-glass latency.

---

## Theory

### Camera: OAK-D Lite
- Uses DepthAI SDK (Python)
- Outputs RGB video + depth map
- Connected via USB 3.0 to RPi 5
- Can run onboard AI inference (object detection, etc.) — optional for now

### Streaming Options

| Method | Latency | Browser Support | Complexity |
|--------|---------|-----------------|------------|
| **MJPEG over HTTP** | ~150–300ms | Universal (img tag) | Very low |
| **HLS (FFmpeg)** | 2–10s | Universal | Low |
| **WebRTC** | ~50–100ms | Universal modern | High |
| **WebSockets + JPEG frames** | ~100–200ms | Universal | Medium |

### Decision
- **Phase 1:** MJPEG over HTTP — simplest, works in any browser, acceptable latency for initial testing
- **Phase 2:** WebRTC — if MJPEG latency is unacceptable for real driving

### MJPEG Pipeline (Phase 1)
```
OAK-D Lite
    │ USB 3.0
    ▼
DepthAI Python script
    │ grabs RGB frames as numpy arrays
    ▼
OpenCV → JPEG encode
    │
    ▼
HTTP multipart/x-mixed-replace stream
    │ WiFi
    ▼
Phone browser <img> tag
```

### WebRTC Pipeline (Phase 2)
```
OAK-D Lite → DepthAI → GStreamer/FFmpeg → WebRTC (aiortc or Janus) → Browser
```

---

## Unit Tests

### File: `tests/camera/test_stream.py`
Runs without hardware — mocks the DepthAI device.

**Run command:**
```bash
pytest tests/camera/test_stream.py -v
```

**Tests:**
| # | Test | Pass Condition |
|---|------|----------------|
| 1 | MJPEG endpoint returns 200 | `GET /stream` → HTTP 200 + `multipart/x-mixed-replace` content-type |
| 2 | Health endpoint | `GET /health` → `{"status":"ok"}` |
| 3 | JPEG frame valid | Each frame boundary parses as valid JPEG |
| 4 | Frame rate ≥ 20fps (mocked) | 20 frames received in ≤ 1.1 seconds |
| 5 | Server survives camera disconnect | DepthAI error → service keeps running, returns 503 on /stream |

### Manual Latency Test (hardware required)
```
1. Open stream in browser: http://<rpi-ip>:8080/stream
2. Hold phone clock in front of OAK-D Lite
3. Screen-record phone showing stream
4. Measure time difference between real clock and stream clock
5. Target: < 200ms
```

### ROS Topic Test
```bash
# With camera node running:
ros2 topic hz /g500/camera/image_raw
# Expected: ~30Hz

ros2 topic echo /g500/camera/compressed --once
# Expected: valid CompressedImage message
```

---

## Attempts

_None yet._

---

## Issue Log

_No issues yet._

---

## Current Status

`[ ] Not Started`

### Next Actions
- [ ] Install DepthAI SDK on RPi 5
- [ ] Write Python script to capture OAK-D Lite RGB frames
- [ ] Add MJPEG HTTP server (Flask or aiohttp)
- [ ] Test stream in phone browser on local WiFi
- [ ] Measure latency
- [ ] Decide if WebRTC is needed based on latency results

---

## Solution

_Not yet solved._
