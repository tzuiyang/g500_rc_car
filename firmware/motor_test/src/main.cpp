/**
 * G500 RC Car — Motor First Test
 *
 * Hardware:
 *   L298N ENA  → Nano D5  (PWM speed)
 *   L298N IN1  → Nano D8  (direction)
 *   L298N IN2  → Nano D9  (direction)
 *   L298N OUT1 → Motor +
 *   L298N OUT2 → Motor -
 *   L298N 12V  → Battery +
 *   L298N GND  → Battery - AND Nano GND  (common ground — required)
 *
 * On boot: runs an automatic test sequence and prints results to Serial.
 * After auto-test: accepts manual commands via Serial Monitor.
 *
 * Serial commands (115200 baud):
 *   F  → Forward full speed
 *   R  → Reverse full speed
 *   S  → Stop
 *   0–9 → Speed step (0=stop, 9=~90% forward)
 */

#include <Arduino.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
const uint8_t PIN_ENA = 5;   // PWM speed (D5)
const uint8_t PIN_IN1 = 8;   // Direction A (D8)
const uint8_t PIN_IN2 = 9;   // Direction B (D9)

// ── Motor helpers ─────────────────────────────────────────────────────────────
void motorStop() {
  analogWrite(PIN_ENA, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
}

void motorForward(uint8_t speed) {   // speed 0–255
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_ENA, speed);
}

void motorReverse(uint8_t speed) {   // speed 0–255
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  analogWrite(PIN_ENA, speed);
}

// ── Ramp helper ───────────────────────────────────────────────────────────────
void rampForward(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    float t = (float)(millis() - start) / durationMs;  // 0.0 → 1.0
    uint8_t spd = (uint8_t)(t * 255);
    motorForward(spd);
    delay(20);
  }
}

void rampReverse(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    float t = (float)(millis() - start) / durationMs;
    uint8_t spd = (uint8_t)(t * 255);
    motorReverse(spd);
    delay(20);
  }
}

// ── Auto test sequence ────────────────────────────────────────────────────────
void runAutoTest() {
  Serial.println(F("=== G500 Motor Test ==="));
  Serial.println(F("Watch the motor physically for each test."));
  Serial.println();

  // TEST 1 — Forward ramp
  Serial.print(F("[TEST 1] Forward ramp 0->255 over 3s... "));
  rampForward(3000);
  Serial.println(F("DONE"));

  // Pause
  motorStop();
  delay(1000);

  // TEST 2 — Full stop
  Serial.print(F("[TEST 2] Stop 1s...                      "));
  motorStop();
  delay(1000);
  Serial.println(F("DONE"));

  // TEST 3 — Reverse ramp
  Serial.print(F("[TEST 3] Reverse ramp 0->255 over 3s... "));
  rampReverse(3000);
  Serial.println(F("DONE"));

  // Pause
  motorStop();
  delay(1000);

  // TEST 4 — Full stop
  Serial.print(F("[TEST 4] Stop 1s...                      "));
  motorStop();
  delay(1000);
  Serial.println(F("DONE"));

  // TEST 5 — PWM steps
  Serial.print(F("[TEST 5] PWM steps forward 64/128/192/255... "));
  uint8_t steps[] = {64, 128, 192, 255};
  for (uint8_t i = 0; i < 4; i++) {
    motorForward(steps[i]);
    delay(1000);
  }
  motorStop();
  Serial.println(F("DONE"));

  Serial.println();
  Serial.println(F("=== Auto-test complete ==="));
  Serial.println(F("Manual commands: F=forward  R=reverse  S=stop  0-9=speed step"));
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);

  motorStop();
  delay(500);   // let L298N power stabilise

  runAutoTest();
}

// ── Loop — manual serial control ─────────────────────────────────────────────
void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'F': case 'f':
      motorForward(255);
      Serial.println(F(">> Forward 100%"));
      break;

    case 'R': case 'r':
      motorReverse(255);
      Serial.println(F(">> Reverse 100%"));
      break;

    case 'S': case 's':
      motorStop();
      Serial.println(F(">> Stop"));
      break;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
      uint8_t spd = (cmd - '0') * 28;   // 0–252 in 10 steps
      motorForward(spd);
      Serial.print(F(">> Forward "));
      Serial.print((cmd - '0') * 10);
      Serial.println(F("%"));
      break;
    }

    default:
      break;   // ignore noise (newlines, etc.)
  }
}
