# Topic: Arduino Nano Firmware

**Status: `[ ] Not Started`**

---

## Problem

The Arduino Nano needs firmware that:
1. Receives drive commands from the Raspberry Pi over USB serial
2. Translates those commands into PWM signals for the ESC (throttle) and servo (steering)
3. Handles a safety failsafe — stop motors if serial communication is lost

---

## Theory

### Serial Protocol (Planned)
Simple JSON or binary packet over USB serial at 115200 baud.

Example JSON command:
```json
{"throttle": 0.5, "steering": -0.3}
```
- `throttle`: -1.0 (full reverse) to 1.0 (full forward), 0 = stop
- `steering`: -1.0 (full left) to 1.0 (full right), 0 = straight

### PWM Output
- Standard RC servo/ESC PWM: 50 Hz, pulse width 1000–2000 µs
  - 1500 µs = neutral (stop / straight)
  - 1000 µs = full reverse / full left
  - 2000 µs = full forward / full right
- Use Arduino `Servo.h` library for both ESC and steering servo

### Failsafe
- Track last command timestamp
- If no valid command received within 500ms → set throttle to 0, steering to neutral
- Prevents runaway car if WiFi drops

### Pin Assignment (TBD)
| Pin | Function |
|-----|----------|
| D9  | ESC (throttle PWM) |
| D10 | Steering servo PWM |

---

## Unit Tests

### Test Sketch: `tests/firmware/protocol_test/protocol_test.ino`
Upload this sketch (not the main firmware) to the Nano to run protocol tests
via the Arduino Serial Monitor at 115200 baud.

**Tests it runs automatically on boot:**
| # | Test | Pass Condition |
|---|------|----------------|
| 1 | Valid JSON parse `{"t":0.5,"s":-0.3}` | PWM D9 = 1750µs, D10 = 1350µs |
| 2 | Neutral command `{"t":0,"s":0}` | Both PWM = 1500µs |
| 3 | Clamp max `{"t":1.5,"s":2.0}` | Both PWM = 2000µs (not over) |
| 4 | Clamp min `{"t":-2.0,"s":-1.5}` | Both PWM = 1000µs (not under) |
| 5 | Malformed JSON `{bad}` | PWM unchanged, no crash |
| 6 | Failsafe timeout | After 600ms silence → PWM = 1500µs, prints `{"status":"failsafe"}` |

**Run command:**
```
1. Upload tests/firmware/protocol_test/protocol_test.ino to Nano
2. Open Serial Monitor @ 115200 baud
3. All PASS lines should appear within 3 seconds
4. Re-upload main firmware when done
```

### Manual Verification (hardware required)
```
1. Flash main firmware
2. Open Serial Monitor
3. Confirm "{"status":"ready"}" appears on boot
4. Send: {"t":1.0,"s":0.0}  → ESC should spin up
5. Send: {"t":0,"s":0}       → ESC should stop
6. Disconnect USB for 600ms  → failsafe should trigger ("{"status":"failsafe"}")
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
- [ ] Decide serial protocol (JSON vs binary)
- [ ] Write basic firmware: receive serial → set servo PWM
- [ ] Add failsafe timer
- [ ] Test with Arduino IDE serial monitor
- [ ] Test end-to-end with RPi serial bridge

---

## Solution

_Not yet solved._
