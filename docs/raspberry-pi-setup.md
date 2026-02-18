# Topic: Raspberry Pi 5 Setup

**Status: `[ ] Not Started`**

---

## Problem

The Raspberry Pi 5 needs to be configured as the car's onboard brain: OS installed, networking configured, USB serial to Arduino enabled, OAK-D Lite recognized, and the server software auto-starting on boot.

---

## Theory

### OS
- Raspberry Pi OS Lite (64-bit, Bookworm) — headless, no desktop needed
- Enable SSH on first boot via `ssh` file in boot partition
- Set hostname: `g500.local` (mDNS for easy discovery)

### Networking
- Option A: RPi acts as WiFi Access Point → phone connects directly (low latency, no internet)
- Option B: RPi connects to existing WiFi → phone on same network (easier setup, needs router)
- Option C: RPi uses 4G LTE dongle → true remote driving anywhere (complex, more latency)
- **Start with Option B for development**, plan for Option A or C for final deployment

### Boot Services
- Use `systemd` service or Docker auto-restart to bring up the car software on boot

### USB Devices
- Arduino Nano: appears as `/dev/ttyUSB0` or `/dev/ttyACM0`
- OAK-D Lite: USB 3.0 port (important for bandwidth), appears as `/dev/bus/usb/...`
- Add user to `dialout` group for serial access: `sudo usermod -aG dialout $USER`

---

## Attempts

_None yet._

---

## Current Status

`[ ] Not Started`

### Next Actions
- [ ] Flash RPi OS to SD card
- [ ] Configure headless SSH + WiFi
- [ ] Set hostname to `g500.local`
- [ ] Install Docker on RPi 5
- [ ] Install Node.js (via nvm or Docker)
- [ ] Test OAK-D Lite detection (`lsusb`)
- [ ] Test Arduino serial communication
- [ ] Set up systemd service or Docker restart policy

---

## Solution

_Not yet solved._
