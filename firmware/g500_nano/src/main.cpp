/**
 * G500 RC Car — Main Nano Firmware (L298N drive)
 *
 * Wiring (confirmed from motor_test):
 *   L298N ENA  → D5  (PWM speed)
 *   L298N IN1  → D8  (direction A)
 *   L298N IN2  → D9  (direction B)
 *   L298N OUT1 → Motor +
 *   L298N OUT2 → Motor -
 *   L298N 12V  → Battery +
 *   L298N GND  → Battery - AND Nano GND  (common ground — required)
 *
 * ── Serial commands (115200 baud) ──────────────────────────────────────────
 *   1–9   Set speed level (1 = slowest ~11%, 9 = full 100%)
 *         Speed takes effect immediately, even while moving.
 *   F     Drive forward at current speed level
 *   B     Drive backward at current speed level
 *   S     Stop (coast)
 *
 * ── JSON mode (for RPi / future ROS bridge) ────────────────────────────────
 *   {"t": <-1.0..1.0>}\n
 *   Positive t → forward, negative t → backward.
 *   Magnitude is multiplied by the current speed level cap.
 *   Speed level still applies as a maximum — JSON cannot exceed it.
 *
 * ── Status replies ─────────────────────────────────────────────────────────
 *   {"status":"ready","speed":5}         on boot
 *   {"status":"forward","speed":5}       on F command
 *   {"status":"backward","speed":5}      on B command
 *   {"status":"stop","speed":5}          on S command
 *   {"status":"speed","level":3}         on speed change
 *   {"status":"failsafe"}                if no command for FAILSAFE_MS
 *
 * ── Failsafe ───────────────────────────────────────────────────────────────
 *   Motor stops automatically if no command is received for 500 ms.
 *   Resumes on next valid command.
 */

#include <Arduino.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
const uint8_t PIN_ENA = 5;
const uint8_t PIN_IN1 = 8;
const uint8_t PIN_IN2 = 9;

// ── Config ────────────────────────────────────────────────────────────────────
const uint32_t BAUD_RATE    = 115200;
const uint32_t FAILSAFE_MS  = 500;

// Speed level 1–9 mapped to PWM 0–255
// level 1 = 28, level 5 = 141, level 9 = 255
const uint8_t SPEED_MAP[10] = {
  0,    // index 0 unused
  28,   // level 1  ~11%
  56,   // level 2  ~22%
  85,   // level 3  ~33%
  113,  // level 4  ~44%
  141,  // level 5  ~55%
  170,  // level 6  ~67%
  198,  // level 7  ~78%
  226,  // level 8  ~89%
  255   // level 9  100%
};

// ── State ─────────────────────────────────────────────────────────────────────
enum Direction { STOPPED, FORWARD, BACKWARD };

uint8_t     speedLevel    = 5;      // default speed level on boot
Direction   currentDir    = STOPPED;
uint32_t    lastCmdTime   = 0;
bool        failsafeActive = false;

// ── Motor helpers ─────────────────────────────────────────────────────────────
void motorStop() {
  analogWrite(PIN_ENA, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  currentDir = STOPPED;
}

void motorForward(uint8_t pwm) {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_ENA, pwm);
  currentDir = FORWARD;
}

void motorBackward(uint8_t pwm) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  analogWrite(PIN_ENA, pwm);
  currentDir = BACKWARD;
}

// Re-apply current direction at current speed level (called after speed change)
void applyCurrentDirection() {
  uint8_t pwm = SPEED_MAP[speedLevel];
  switch (currentDir) {
    case FORWARD:  motorForward(pwm);  break;
    case BACKWARD: motorBackward(pwm); break;
    case STOPPED:  motorStop();        break;
  }
}

// ── JSON status reply ─────────────────────────────────────────────────────────
void printStatus(const char* status) {
  Serial.print(F("{\"status\":\""));
  Serial.print(status);
  Serial.print(F("\",\"speed\":"));
  Serial.print(speedLevel);
  Serial.println(F("}"));
}

void printSpeedChange() {
  Serial.print(F("{\"status\":\"speed\",\"level\":"));
  Serial.print(speedLevel);
  Serial.println(F("}"));
}

// ── JSON command parser (for RPi / ROS bridge) ────────────────────────────────
// Minimal parser — avoids ArduinoJson dependency for main firmware.
// Looks for "t": followed by a float value.
bool parseJsonThrottle(const String& line, float& outT) {
  int idx = line.indexOf("\"t\"");
  if (idx < 0) idx = line.indexOf("'t'");
  if (idx < 0) return false;
  int colon = line.indexOf(':', idx);
  if (colon < 0) return false;
  outT = line.substring(colon + 1).toFloat();
  return true;
}

// ── Handle one serial command ─────────────────────────────────────────────────
void handleCommand(const String& raw) {
  String line = raw;
  line.trim();
  if (line.length() == 0) return;

  lastCmdTime   = millis();
  failsafeActive = false;

  // ── JSON mode ──────────────────────────────────────────────────────────────
  if (line.startsWith("{")) {
    float t = 0.0f;
    if (parseJsonThrottle(line, t)) {
      t = constrain(t, -1.0f, 1.0f);
      uint8_t maxPwm = SPEED_MAP[speedLevel];
      uint8_t pwm    = (uint8_t)(abs(t) * maxPwm);
      if (t > 0.01f)       { motorForward(pwm);  printStatus("forward");  }
      else if (t < -0.01f) { motorBackward(pwm); printStatus("backward"); }
      else                 { motorStop();         printStatus("stop");     }
    }
    return;
  }

  // ── Single-char commands ───────────────────────────────────────────────────
  char cmd = line.charAt(0);

  if (cmd >= '1' && cmd <= '9') {
    speedLevel = cmd - '0';
    applyCurrentDirection();   // update speed immediately if already moving
    printSpeedChange();
    return;
  }

  switch (cmd) {
    case 'F': case 'f':
      motorForward(SPEED_MAP[speedLevel]);
      printStatus("forward");
      break;

    case 'B': case 'b':
    case 'R': case 'r':        // R kept as alias from motor_test
      motorBackward(SPEED_MAP[speedLevel]);
      printStatus("backward");
      break;

    case 'S': case 's':
      motorStop();
      printStatus("stop");
      break;

    default:
      break;   // ignore unknown / newlines
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(BAUD_RATE);

  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);

  motorStop();
  lastCmdTime = millis();

  printStatus("ready");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Read serial command
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }

  // Failsafe watchdog
  if (!failsafeActive && (millis() - lastCmdTime > FAILSAFE_MS)) {
    motorStop();
    failsafeActive = true;
    Serial.println(F("{\"status\":\"failsafe\"}"));
  }
}
