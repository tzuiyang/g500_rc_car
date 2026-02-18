# Topic: Serial Communication (RPi 5 ↔ Arduino Nano)

**Status: `[ ] Not Started`**

---

## Problem

The Raspberry Pi 5 needs to reliably send drive commands to the Arduino Nano over USB serial. The Node.js server receives WebSocket messages from the phone and must forward them to the Arduino fast enough for responsive steering.

---

## Theory

### Physical Connection
- Arduino Nano → USB-A (or Mini-USB depending on Nano version) → RPi 5 USB port
- The Nano appears as a serial device: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Baud rate: 115200 (standard for Arduino, fast enough for control commands)

### Node.js Serial Library
- **serialport** npm package — most mature, cross-platform
- API: `new SerialPort({ path: '/dev/ttyUSB0', baudRate: 115200 })`
- Works in Node.js on RPi

### Protocol Design
Keep it simple and low-overhead. Options:

**Option A — JSON (human-readable, easy debug)**
```
{"t":0.5,"s":-0.3}\n
```
Pro: easy to debug with serial monitor. Con: slightly more bytes.

**Option B — Binary (2-byte packet)**
```
[0xAA] [throttle_byte] [steering_byte] [checksum]
```
Pro: minimal bytes, fast parse on Arduino. Con: harder to debug.

**Decision: Start with JSON, optimize to binary only if latency is an issue.**

### Timing
- WebSocket command arrives at server → serialize → write to serial
- Arduino parses → updates PWM → next PWM cycle (20ms at 50Hz)
- Total added latency: < 5ms for serial at 115200 baud for short packets
- This is well within acceptable range

### Error Handling
- Wrap serial writes in try/catch
- If serial port disconnects (Arduino unplugged), log error and attempt reconnect every 2s
- Arduino firmware has its own failsafe (stop if no command for 500ms)

---

## Unit Tests

### File: `tests/server/serial.test.js`
Run without hardware — uses a mock serial port.

**Run command:**
```bash
npm test --workspace=server
```

**Tests:**
| # | Test | Pass Condition |
|---|------|----------------|
| 1 | Auto-detect port (mock) | Returns first `/dev/ttyUSB*` or `/dev/ttyACM*` found |
| 2 | Send valid command | `{"t":0.5,"s":-0.3}\n` written to mock port |
| 3 | Clamp throttle > 1.0 | Written value capped at 1.0 |
| 4 | Clamp steering < -1.0 | Written value floored at -1.0 |
| 5 | Port disconnect → reconnect | Reconnect attempt fires within 3s |
| 6 | No port found → mock mode | Server starts, logs warning, no crash |
| 7 | Last client disconnects | `{"t":0,"s":0}` written (safety stop) |

### Hardware Test (Arduino attached)
```bash
# From RPi or dev machine with Nano plugged in:
node scripts/serial_loopback_test.js
# Sends 10 commands, verifies Nano echoes status back
# Expected output: "10/10 commands acknowledged"
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
- [ ] Install `serialport` npm package in server
- [ ] Detect serial port path dynamically (scan `/dev/ttyUSB*` and `/dev/ttyACM*`)
- [ ] Write serial bridge module: WebSocket message → serial write
- [ ] Test with Arduino echo sketch
- [ ] Test with full Arduino firmware
- [ ] Measure round-trip latency

---

## Solution

_Not yet solved._
