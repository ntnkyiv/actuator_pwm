// LinearActuator.cpp — САМИЙ ПРОСТИЙ І НАДІЙНИЙ СПОСІБ
#include <Arduino.h>
#include "LinearActuator.h"
#include "config.h"

extern Preferences preferences;

int linearSpeed = 0;      // -255 = назад, 255 = вперед, 0 = стоп
bool linearExtended = false;

// Піни — можна будь-які вільні (наприклад 12 і 13)
#define LIN_PIN_IN1  1   // GPIO1
#define LIN_PIN_IN2  2   // GPIO2

#define PWM_FREQ     25000   // 25 кГц — безшумно
#define PWM_RES      8       // 0–255
#define PWM_CH1      0
#define PWM_CH2      1

static uint32_t brakeDelayMs = 20000;         // за замовчуванням 20 сек
static unsigned long brakeStartTime = 0;
static bool brakeScheduled = false;

static void loadBrakeTime() {
  preferences.begin("linear", false);
  if (!preferences.isKey("brake_time")) preferences.putInt("brake_time", 20000);
  brakeDelayMs = preferences.getUInt("brake_time");
  preferences.end();
}

static void saveBrakeTime() {
  preferences.begin("linear", false);
  preferences.putUInt("brake_time", brakeDelayMs);
  preferences.end();
}

void linearInit() {
  pinMode(LIN_PIN_IN1, OUTPUT);
  pinMode(LIN_PIN_IN2, OUTPUT);
  digitalWrite(LIN_PIN_IN1, LOW);
  digitalWrite(LIN_PIN_IN2, LOW);

  // Новий API Arduino-ESP32 3.x — працює на всіх пінах!
  ledcAttach(LIN_PIN_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(LIN_PIN_IN2, PWM_FREQ, PWM_RES);

  loadBrakeTime();
  Serial.printf("Лінійний актуатор PWM готовий (IN1=GPIO%d, IN2=GPIO%d, %u кГц)\n", 
                LIN_PIN_IN1, LIN_PIN_IN2, PWM_FREQ / 1000);
}

void linearSetSpeed(int speed) {
  speed = constrain(speed, -255, 255);
  linearSpeed = speed;
  linearExtended = (speed > 0);

  if (speed > 0) {
    ledcWrite(LIN_PIN_IN1, speed);   // вперед
    ledcWrite(LIN_PIN_IN2, 0);
  }
  else if (speed < 0) {
    ledcWrite(LIN_PIN_IN1, 0);
    ledcWrite(LIN_PIN_IN2, -speed);  // назад
  }
  else {
    ledcWrite(LIN_PIN_IN1, 0);
    ledcWrite(LIN_PIN_IN2, 0);
  }

  if (speed != 0) {
    brakeStartTime = millis();
    brakeScheduled = true;
  } else {
    brakeScheduled = false;
  }
}

// === Автогальмування (викликати з loop()!) ===
void linearAutoBrake() {
  if (!brakeScheduled) return;
  if (millis() - brakeStartTime >= brakeDelayMs) {
    linearBrake();
    brakeScheduled = false;
    Serial.printf("Автогальмування! Час: %u мс\n", brakeDelayMs);
  }
}


void linearExtend()   { linearSetSpeed(255); }
void linearRetract()  { linearSetSpeed(-255); }
void linearBrake()    { ledcWrite(LIN_PIN_IN1, 255); ledcWrite(LIN_PIN_IN2, 255); linearSpeed = 0; brakeScheduled = false; }
void linearSleep()    { ledcWrite(LIN_PIN_IN1, 0);   ledcWrite(LIN_PIN_IN2, 0);   linearSpeed = 0; brakeScheduled = false; }
void linearToggle()   { linearSpeed > 0 ? linearRetract() : linearExtend(); }

void linearSetBrakeTime(uint32_t ms) {
  if (ms < 1000) ms = 1000;
  if (ms > 300000) ms = 300000;
  brakeDelayMs = ms;
  saveBrakeTime();
}

uint32_t linearGetBrakeTime() {
  return brakeDelayMs;
}