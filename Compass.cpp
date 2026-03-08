#include "Compass.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <Preferences.h>
#include "StepperControl.h"
#include "LinearActuator.h"
#include "esp_task_wdt.h"

#define STEPPER_ENABLE_PIN D10

extern AccelStepper stepper;
extern float stepper_ratio;

Adafruit_BNO08x bno08x(-1); // без reset-піна

bool compassFound = false;

float currentPitch = 0.0f;
float currentRoll  = 0.0f;
float currentYaw   = 0.0f;
float currentTemp  = 0.0f;

// Діагностичні змінні
static uint32_t bnoLastOkMs       = 0;
static uint32_t bnoFailCount      = 0;
static uint8_t  bnoAddress        = 0;
static uint8_t  bnoRecoveryStage  = 0;
static uint32_t bnoLastRecoveryMs = 0;


// === ДОПОМІЖНІ ===

void logStep(String msg) {
  Serial.println(msg);
}

void recoverI2C(int sdaPin, int sclPin) {
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
  delay(10);

  if (digitalRead(sdaPin) == LOW) {
    logStep("I2C BUS STUCK! Recovering...");
    pinMode(sclPin, OUTPUT);
    for (int i = 0; i < 10; i++) {
      digitalWrite(sclPin, HIGH); delayMicroseconds(10);
      digitalWrite(sclPin, LOW);  delayMicroseconds(10);
    }
    pinMode(sclPin, INPUT_PULLUP);
    delay(5);
  }

  pinMode(sdaPin, OUTPUT); digitalWrite(sdaPin, LOW);
  pinMode(sclPin, OUTPUT); digitalWrite(sclPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(sdaPin, HIGH);
  delayMicroseconds(5);
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
}

// === ІНІЦІАЛІЗАЦІЯ ===

void compassInit() {
  logStep("=== BNO08x INIT START ===");

  pinMode(STEPPER_ENABLE_PIN, OUTPUT);
  digitalWrite(STEPPER_ENABLE_PIN, HIGH);
  delay(500);

  recoverI2C(5, 6);
  Wire.end();
  Wire.begin(5, 6, 100000);
  Wire.setTimeOut(200);
  delay(100);

  // I2C сканер
  logStep("--- I2C Bus Scan ---");
  bool anyFound = false;
  for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      char buf[24];
      sprintf(buf, "  Found: 0x%02X", addr);
      logStep(String(buf));
      anyFound = true;
    }
  }
  if (!anyFound) logStep("  No I2C devices found!");
  logStep("--- Scan Done ---");

  compassFound = false;

  for (int attempt = 1; attempt <= 5; attempt++) {
    esp_task_wdt_reset();
    logStep("Attempt " + String(attempt) + "/5...");

    uint8_t bnoAddr = 0;
    Wire.beginTransmission(0x4A);
    if (Wire.endTransmission() == 0) bnoAddr = 0x4A;
    else {
      Wire.beginTransmission(0x4B);
      if (Wire.endTransmission() == 0) bnoAddr = 0x4B;
    }

    if (bnoAddr != 0) {
      logStep("BNO08x found at 0x" + String(bnoAddr, HEX));
      esp_task_wdt_reset();
      if (bno08x.begin_I2C(bnoAddr)) {
        logStep("BNO08x Init: SUCCESS");
        bno08x.enableReport(SH2_ROTATION_VECTOR);
        compassFound = true;
        bnoAddress   = bnoAddr;
        logStep(">>> BNO08x READY");
        break;
      } else {
        logStep("BNO08x Init: FAILED");
      }
    } else {
      logStep("BNO08x not found!");
    }

    if (attempt < 5) {
      logStep("Waiting 3s before retry...");
      for (int w = 0; w < 30; w++) {
        delay(100);
        esp_task_wdt_reset();
      }
      recoverI2C(5, 6);
      Wire.begin(5, 6, 100000);
    }
  }

  if (!compassFound) {
    logStep("CRITICAL: BNO08x not found after 5 attempts.");
  }

  digitalWrite(STEPPER_ENABLE_PIN, LOW);
}

// === ОНОВЛЕННЯ ДАНИХ ===

void getBNODiagnostics(uint32_t &outLastOkMs, uint32_t &outFailCount,
                       uint8_t &outI2C4A, uint8_t &outI2C4B, uint8_t &outRecoveryStage) {
  outLastOkMs      = bnoLastOkMs;
  outFailCount     = bnoFailCount;
  outRecoveryStage = bnoRecoveryStage;
  Wire.beginTransmission(0x4A);
  outI2C4A = Wire.endTransmission();
  Wire.beginTransmission(0x4B);
  outI2C4B = Wire.endTransmission();
}

void updatePRY() {
  if (!compassFound) return;

  sh2_SensorValue_t sv;
  if (!bno08x.getSensorEvent(&sv)) {
    bnoFailCount++;

    if (bnoLastOkMs == 0) return; // ще не було жодного успішного читання

    uint32_t now     = millis();
    uint32_t staleMs = now - bnoLastOkMs;

    // Через 2с — м'яке відновлення: повторний enableReport
    if (staleMs > 2000 && bnoRecoveryStage == 0 && (now - bnoLastRecoveryMs) > 10000) {
      bno08x.enableReport(SH2_ROTATION_VECTOR);
      bnoRecoveryStage  = 1;
      bnoLastRecoveryMs = now;
    }
    // Через 30с — жорстке відновлення: повний begin_I2C
    else if (staleMs > 30000 && bnoRecoveryStage == 1 && (now - bnoLastRecoveryMs) > 10000) {
      Wire.beginTransmission(bnoAddress);
      if (Wire.endTransmission() == 0) {
        if (bno08x.begin_I2C(bnoAddress)) {
          bno08x.enableReport(SH2_ROTATION_VECTOR);
        }
      }
      bnoRecoveryStage  = 2;
      bnoLastRecoveryMs = now;
    }

    return;
  }

  if (sv.sensorId != SH2_ROTATION_VECTOR) return;

  bnoFailCount     = 0;
  bnoRecoveryStage = 0;
  bnoLastOkMs      = millis();

  float qw = sv.un.rotationVector.real;
  float qx = sv.un.rotationVector.i;
  float qy = sv.un.rotationVector.j;
  float qz = sv.un.rotationVector.k;

  // Yaw: мінус для CW = зростання (компасна конвенція)
  float yaw   = -atan2(2.0f*(qw*qz + qx*qy), 1.0f - 2.0f*(qy*qy + qz*qz)) * 180.0f / PI - 180.0f;
  float roll  =  asin(constrain(2.0f*(qw*qy - qz*qx), -1.0f, 1.0f)) * 180.0f / PI;
  float pitch =  atan2(2.0f*(qw*qx + qy*qz), 1.0f - 2.0f*(qx*qx + qy*qy)) * 180.0f / PI;
  if (yaw < 0) yaw += 360.0f;

  currentPitch = roundf(pitch * 10) / 10.0f;
  currentRoll  = roundf(roll  * 10) / 10.0f;
  currentYaw   = roundf(yaw   * 10) / 10.0f;
}

// === КАЛІБРУВАННЯ MOTCAL ===

static void waitStepper() {
  while (stepper.distanceToGo() != 0) {
    stepper.run();
    esp_task_wdt_reset();
    yield();
  }
}

static void runLinear(int speed, uint32_t timeMs) {
  linearSetSpeed(speed);
  unsigned long start = millis();
  while (millis() - start < timeMs) {
    esp_task_wdt_reset();
    yield();
  }
  linearSetSpeed(0);
}

void runBNO08xCalibration(int cycles) {
  Serial.printf("=== MOTCAL КАЛІБРУВАННЯ: %d цикл(ів) ===\n", cycles);
  if (stepper_ratio <= 0) { Serial.println("ERR: stepper_ratio = 0"); return; }

  long steps180 = (long)(stepper_ratio * 180.0f);
  long steps360 = (long)(stepper_ratio * 360.0f);
  uint32_t linTime = brakeDelayMs;

  // Підняти антену один раз перед усіма циклами
  runLinear(255, linTime);
  delay(300);

  for (int c = 1; c <= cycles; c++) {
    Serial.printf("Цикл %d/%d\n", c, cycles);
    esp_task_wdt_reset();

    // 1. Повернути на 180° проти годинникової
    stepper.move(-steps180);
    waitStepper();
    delay(200);

    // 2. Опустити антену вниз
    runLinear(-255, linTime);
    delay(300);

    // 3. Повернути на 360° за годинниковою
    stepper.move(steps360);
    waitStepper();
    delay(200);

    // 4. Підняти антену вгору
    runLinear(255, linTime);
    delay(300);

    // 5. Повернути на 180° проти годинникової
    stepper.move(-steps180);
    waitStepper();
    delay(200);
  }

  // Опустити антену після завершення
  runLinear(-255, linTime);
  delay(300);

  Serial.println("=== КАЛІБРУВАННЯ ЗАВЕРШЕНО ===");
}

// === КЕРУВАННЯ ===

extern float azimuth;

void moveToAzimuth(float target) {
  if (!compassFound) return;
  target = fmod(fmod(target, 360.0) + 360.0, 360.0);
  azimuth = target;
  float diff = target - currentYaw;
  if (diff > 180.0f)  diff -= 360.0f;
  if (diff < -180.0f) diff += 360.0f;
  stepper.move((long)(diff * stepper_ratio));
}

void setMotorState(bool enabled) {
  digitalWrite(STEPPER_ENABLE_PIN, enabled ? LOW : HIGH);
}
