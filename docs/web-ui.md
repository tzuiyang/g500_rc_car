# Topic: Web UI (Phone FPV Controller)

**Status: `[ ] Not Started`**

---

## Problem

Users need a phone-friendly web interface to:
1. See the live FPV camera feed
2. Control the car (throttle + steering) via on-screen joystick or tilt
3. Connect/disconnect cleanly
4. See connection status and basic telemetry

---

## Theory

### UI Requirements
- **Mobile-first** — designed for phone browser portrait/landscape
- **No app install** — runs in browser, no native app needed
- **Low latency input** — touch events → WebSocket → server as fast as possible
- **Responsive** — works on iOS Safari and Android Chrome

### Tech Stack Options

| Option | Pros | Cons |
|--------|------|------|
| **Plain HTML/JS** | Zero build tooling, works anywhere | Harder to maintain as complexity grows |
| **React (Vite)** | Component model, hot reload in dev | Needs build step |
| **Next.js** | Full-stack option (API + UI together) | More complex, RPi overhead |

**Decision: Plain HTML/JS for v1** — keeps it simple, no build step needed on RPi, deploy with any static file server (Express, Nginx).

### UI Layout (Phone Landscape)
```
┌─────────────────────────────────────────┐
│  [STATUS: Connected ●]                  │
├─────────────────────────────────────────┤
│                                         │
│         CAMERA STREAM (FPV)            │
│                                         │
├──────────────┬──────────────────────────┤
│              │                          │
│  [STEERING]  │      [THROTTLE]          │
│  joystick    │      slider/joystick     │
│              │                          │
└──────────────┴──────────────────────────┘
```

### Input Methods
- **Virtual Joystick** — nipplejs library (touch joystick)
- **Tilt control** — DeviceOrientation API (optional, toggle-able)
- **Keyboard** — WASD for desktop testing

### WebSocket Protocol (Client → Server)
```json
{"throttle": 0.0, "steering": 0.0}
```
- Send on every touch/input event, throttled to max 30 messages/sec (33ms)
- Send `{"throttle": 0, "steering": 0}` on touch release

### Camera Display
- Phase 1: `<img>` tag pointing to MJPEG stream URL
- Phase 2: WebRTC `<video>` element

---

## Attempts

_None yet._

---

## Current Status

`[ ] Not Started`

### Next Actions
- [ ] Create `ui/index.html` with basic layout
- [ ] Add nipplejs virtual joystick
- [ ] Implement WebSocket connection to server
- [ ] Display MJPEG stream
- [ ] Test on iOS Safari and Android Chrome
- [ ] Add connection status indicator
- [ ] Test input latency

---

## Solution

_Not yet solved._
