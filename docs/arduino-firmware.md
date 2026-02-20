# Topic: Arduino Nano Firmware

**Status: `[ ] In Progress`**

---

## Problem

The Arduino Nano needs firmware that:
1. Drives a DC motor via L298N (throttle PWM + direction pins)
2. Supports a speed level 1–9 that caps maximum motor power
3. Accepts manual serial commands (F / B / S / 1–9) for standalone testing
4. Accepts JSON commands from the Raspberry Pi for autonomous/ROS control
5. Stops the motor automatically (failsafe) if serial communication is lost

---

## Theory

### Hardware — L298N + DC Motor
Confirmed working from `docs/motor-first-test.md`.

| Pin | Connected to | Purpose |
|-----|-------------|---------|
| D5  | L298N ENA   | PWM speed (0–255) |
| D8  | L298N IN1   | Direction bit A |
| D9  | L298N IN2   | Direction bit B |

L298N direction truth table:
| IN1 | IN2 | Motor |
|-----|-----|-------|
| HIGH | LOW | Forward |
| LOW | HIGH | Backward |
| LOW | LOW | Stop (coast) |

### Speed Levels 1–9
Speed level caps maximum PWM. Changing level while moving updates speed immediately.

| Level | PWM  | % Power |
|-------|------|---------|
| 1     | 28   | 11% |
| 2     | 56   | 22% |
| 3     | 85   | 33% |
| 4     | 113  | 44% |
| 5     | 141  | 55% (default) |
| 6     | 170  | 67% |
| 7     | 198  | 78% |
| 8     | 226  | 89% |
| 9     | 255  | 100% |

### Serial Protocol
All commands at **115200 baud**, newline-terminated.

**Manual commands (human / serial monitor):**
| Command | Action |
|---------|--------|
| `1`–`9` | Set speed level. Updates immediately, even while moving |
| `F` | Forward at current speed level |
| `B` | Backward at current speed level |
| `S` | Stop |

**JSON commands (RPi / ROS bridge):**
```json
{"t": 0.75}
```
- `t` range: -1.0 (full backward) → 0 (stop) → 1.0 (full forward)
- Magnitude is scaled by the current speed level cap — cannot exceed it
- `B` is kept as alias for `R` (backwards compat with motor_test)

**Status replies (JSON, always sent back):**
```json
{"status":"ready","speed":5}       ← on boot
{"status":"forward","speed":5}     ← on F
{"status":"backward","speed":5}    ← on B
{"status":"stop","speed":5}        ← on S
{"status":"speed","level":3}       ← on 1–9
{"status":"failsafe"}              ← watchdog triggered
```

### Failsafe
- If no valid command received for **500 ms** → motor stops, prints failsafe status
- Next valid command clears failsafe and resumes normally

### No External Libraries
The main firmware uses no external libraries (no ArduinoJson, no Servo.h).
JSON parsing is a simple `indexOf` scan — fast, zero memory overhead.

---

## Unit Tests

### Manual Verification (hardware required)
```
1. Flash firmware/g500_nano/ to Nano:
   pio run --target upload  (from firmware/g500_nano/)

2. Open serial monitor:
   pio device monitor --port COM6 --baud 115200

3. Confirm on boot:
   {"status":"ready","speed":5}

4. Test speed levels:
   Send: 3   →  {"status":"speed","level":3}
   Send: F   →  {"status":"forward","speed":3}  motor spins slowly
   Send: 7   →  {"status":"speed","level":7}    motor speeds up immediately
   Send: S   →  {"status":"stop","speed":7}     motor stops

5. Test forward/backward:
   Send: F   →  motor forward
   Send: B   →  motor reverses
   Send: S   →  stop

6. Test failsafe:
   Send: F   →  motor starts
   Disconnect USB data (or stop sending) for 600ms
   Motor should stop, serial prints {"status":"failsafe"}

7. Test JSON mode:
   Send: {"t":0.5}   →  forward at 50% of current speed level
   Send: {"t":-1.0}  →  full backward (capped at speed level)
   Send: {"t":0}     →  stop
```

### Speed Level Test Checklist
| Send | Expected reply | Motor behaviour |
|------|---------------|----------------|
| `5`  | `{"status":"speed","level":5}` | Speed stored |
| `F`  | `{"status":"forward","speed":5}` | Spins ~55% |
| `9`  | `{"status":"speed","level":9}` | Immediately faster |
| `1`  | `{"status":"speed","level":1}` | Immediately slower |
| `S`  | `{"status":"stop","speed":1}` | Stops |
| `B`  | `{"status":"backward","speed":1}` | Slow reverse |

---

## Attempts

### Attempt 1 — Old ESC/Servo firmware scaffold (2026-02-18)
**What we did:** Wrote `firmware/g500_nano/g500_nano.ino` targeting ESC+Servo via Servo.h.
**Result:** Outdated — hardware confirmed as L298N, not ESC. Kept as reference only.

### Attempt 2 — PlatformIO rewrite for L298N (2026-02-20)
**What we did:** Created `firmware/g500_nano/platformio.ini` + `src/main.cpp` targeting L298N wiring confirmed in motor_test. Added speed level 1–9 system, JSON+manual serial protocol, failsafe watchdog. No external libraries.
**Result:** Written and flashed successfully — 6952 bytes verified on COM6 in 5.02s.
**Boot confirmed:** `{"status":"ready","speed":5}` received immediately. Failsafe fires after 500ms silence as expected.

---

## Issue Log

_No issues yet. See `docs/motor-first-test.md` for ISSUE-001 (bootloader mismatch — fixed)._

---

## Current Status

`[ ] In Progress` — Firmware written. Pending flash and physical test.

### Next Actions
- [x] Confirm hardware wiring (from motor_test)
- [x] Write PlatformIO firmware with speed levels
- [x] Flash `firmware/g500_nano/` — 6952 bytes verified
- [x] Boot reply confirmed: `{"status":"ready","speed":5}`
- [x] Failsafe confirmed: fires after 500ms
- [ ] Full manual verification checklist (F/B/S/1-9 with motor physically spinning)
- [ ] Test JSON mode end-to-end with RPi
- [ ] Mark solved when full checklist passes

---

## Solution

_Partially solved — firmware flashed and boot confirmed. Full physical drive test pending._
