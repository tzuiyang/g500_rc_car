# Topic: Motor First Test Run (L298N + DC Motor)

**Status: `[ ] In Progress`**

---

## Problem

Verify the DC motor spins correctly under Arduino Nano PWM control via L298N
before integrating into the full car firmware. This is a standalone hardware
sanity check — no RPi, no ROS, no serial commands from a host.

---

## Theory

### L298N Motor Driver
The L298N is an H-bridge driver that controls direction and speed of DC motors.

| L298N Pin | Connected To | Purpose |
|-----------|-------------|---------|
| OUT1 | Motor + (red) | Motor terminal A |
| OUT2 | Motor − (black) | Motor terminal B |
| 12V (VCC) | Battery + | Motor power supply (up to 35V) |
| GND | Battery − + Nano GND | Common ground |
| ENA | Nano D5 | PWM speed control (0–255) |
| IN1 | Nano D8 | Direction bit A |
| IN2 | Nano D9 | Direction bit B |

> **Important:** GND must be shared between the Nano and L298N/battery.
> Without a common ground, logic signals have no reference and the motor will
> not respond.

### Direction Truth Table
| IN1 | IN2 | Motor behaviour |
|-----|-----|----------------|
| HIGH | LOW | Forward |
| LOW | HIGH | Reverse |
| LOW | LOW | Coast (free spin) |
| HIGH | HIGH | Brake (short-circuit) |

### Speed Control
ENA is a PWM pin (Nano D5, Timer 0, 490 Hz by default).
`analogWrite(ENA, value)` where:
- `0` = stopped
- `128` = ~50% speed
- `255` = full speed

### Pin Selection Notes
- **D5** is a hardware PWM pin on the Nano — correct choice for ENA.
- **D8, D9** are standard digital pins — correct for direction control (IN1, IN2).
- D9 is also PWM-capable but used as digital here — fine.

### Why PlatformIO?
- Can be used on any OS (Windows, macOS, Linux, RPi)
- Works in VSCode with full IntelliSense
- No need to install Arduino IDE separately
- Upload via: `pio run --target upload`

---

## Unit Tests

### Automated Test (no oscilloscope needed)
The test firmware runs a scripted sequence on boot and prints PASS/FAIL to
Serial Monitor at 115200 baud.

**Run command:**
```bash
# From firmware/motor_test/ directory:
pio run --target upload
# Then open serial monitor:
pio device monitor --baud 115200
```

**Expected serial output:**
```
=== G500 Motor Test ===
[TEST 1] Forward ramp 0->255... RUNNING 3s
[TEST 2] Stop...                DONE
[TEST 3] Reverse ramp 0->255... RUNNING 3s
[TEST 4] Stop...                DONE
[TEST 5] PWM steps 64/128/192/255... DONE
=== All tests complete. Type F/R/S for manual control ===
```

**Visual PASS criteria (watch the motor):**
| Test | Expected |
|------|----------|
| TEST 1 | Motor spins forward, gradually speeds up |
| TEST 2 | Motor stops |
| TEST 3 | Motor spins opposite direction, speeds up |
| TEST 4 | Motor stops |
| TEST 5 | Motor steps through 4 speed levels, pausing 1s each |

**Manual serial commands (after auto-test):**
| Command | Action |
|---------|--------|
| `F` | Forward full speed |
| `R` | Reverse full speed |
| `S` | Stop |
| `0`–`9` | Set speed 0%–90% (forward) |

### FAIL indicators to log as issues:
- Motor does not spin at all → check wiring / common GND / ENA PWM
- Motor only spins one direction → check IN1/IN2 wiring
- Motor spins backwards in TEST 1 → swap OUT1/OUT2 wires on L298N
- Nano resets during motor start → power draw too high, check 5V supply
- Serial output never appears → wrong COM port or baud rate

---

## Attempts

### Attempt 1 — Initial wiring + PlatformIO sketch (2026-02-20)
**What we did:** Defined wiring, created PlatformIO project at
`firmware/motor_test/`, wrote auto-test + manual control firmware.
**Result:** Code written. Not yet flashed — awaiting hardware test.
**Next step:** Flash to Nano, run test sequence, log results here.

---

## Issue Log

_No issues yet._

---

## Current Status

`[ ] In Progress` — Firmware written. Pending hardware flash and test.

### Next Actions
- [ ] Flash `firmware/motor_test/` to Nano via `pio run --target upload`
- [ ] Open serial monitor, verify auto-test output
- [ ] Watch motor physically, confirm forward/reverse/stop
- [ ] Log any issues above using ISSUE FORMAT
- [ ] If all pass → mark `[x] Solved` and proceed to servo test

---

## Solution

_Not yet solved — pending hardware test._

---

## Wiring Diagram (ASCII)

```
  Battery 12V
  +────────────────────────────┐
  │                            │
  │  ┌──────────────┐          │ L298N
  └─▶│ 12V      OUT1│──────── Motor+
     │          OUT2│──────── Motor−
  ┌─▶│ GND      ENA │◀── D5 (PWM)
  │  │          IN1 │◀── D8
  │  │          IN2 │◀── D9
  │  └──────────────┘
  │
  ├─── Nano GND
  │
  └─── Battery −
```
