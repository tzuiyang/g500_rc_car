/**
 * G500 RC Car — Arduino Nano Firmware
 *
 * Receives JSON drive commands from Raspberry Pi over USB Serial.
 * Outputs PWM to ESC (throttle) and steering servo.
 *
 * Protocol (from RPi):
 *   {"t": <-1.0..1.0>, "s": <-1.0..1.0>}\n
 *   t = throttle  (-1 full reverse, 0 stop, 1 full forward)
 *   s = steering  (-1 full left,    0 straight, 1 full right)
 *
 * PWM mapping:
 *   1000 µs = full reverse / full left
 *   1500 µs = neutral
 *   2000 µs = full forward / full right
 */

#include <Servo.h>
#include <ArduinoJson.h>   // https://arduinojson.org/ — install via Library Manager

// ── Pin assignments ──────────────────────────────────────────────────────────
#define PIN_ESC     9   // ESC (throttle)
#define PIN_SERVO   10  // Steering servo

// ── Config ───────────────────────────────────────────────────────────────────
#define BAUD_RATE       115200
#define FAILSAFE_MS     500     // Stop if no command for this many ms
#define PWM_NEUTRAL     1500
#define PWM_MIN         1000
#define PWM_MAX         2000

// ── Globals ──────────────────────────────────────────────────────────────────
Servo esc;
Servo steer;

unsigned long lastCmdTime = 0;
bool failsafeActive = false;

// ── Helpers ──────────────────────────────────────────────────────────────────
int valueToPwm(float value) {
  // value: -1.0 .. 1.0  →  PWM: 1000 .. 2000 µs
  value = constrain(value, -1.0f, 1.0f);
  return (int)(PWM_NEUTRAL + value * (PWM_MAX - PWM_NEUTRAL));
}

void applyCommand(float t, float s) {
  esc.writeMicroseconds(valueToPwm(t));
  steer.writeMicroseconds(valueToPwm(s));
}

void applyFailsafe() {
  esc.writeMicroseconds(PWM_NEUTRAL);
  steer.writeMicroseconds(PWM_NEUTRAL);
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(BAUD_RATE);

  esc.attach(PIN_ESC, PWM_MIN, PWM_MAX);
  steer.attach(PIN_SERVO, PWM_MIN, PWM_MAX);

  applyFailsafe();  // start safe
  lastCmdTime = millis();

  Serial.println("{\"status\":\"ready\"}");
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  // ── Parse incoming serial ──────────────────────────────────────────────────
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(doc, line);

    if (!err) {
      float t = doc["t"] | 0.0f;
      float s = doc["s"] | 0.0f;
      applyCommand(t, s);
      lastCmdTime = millis();
      failsafeActive = false;
    }
    // Silently ignore malformed packets
  }

  // ── Failsafe watchdog ──────────────────────────────────────────────────────
  if (!failsafeActive && (millis() - lastCmdTime > FAILSAFE_MS)) {
    applyFailsafe();
    failsafeActive = true;
    Serial.println("{\"status\":\"failsafe\"}");
  }
}
