#pragma once
#include <Arduino.h>
#include "StepperControl.h"
#include "config.h"
#include "Compass.h"

void stepperInit() {
  loadStepperSettings();
  stepper.setCurrentPosition(0);
}

void loadStepperSettings() {
  // Відкриваємо в режимі читання/запису (false), 
  // щоб мати можливість зберегти значення за замовчуванням
  preferences.begin("compass", false);

  // Функція для перевірки та ініціалізації кожного параметра
  // Якщо ключа немає, він створюється зі значенням за замовчуванням
  if (!preferences.isKey("pulsewidth")) preferences.putInt("pulsewidth", 20);
  if (!preferences.isKey("maxspeed")) preferences.putInt("maxspeed", 5000);
  if (!preferences.isKey("acceleration")) preferences.putInt("acceleration", 5000);
  if (!preferences.isKey("microstep")) preferences.putFloat("microstep", 32.0);
  if (!preferences.isKey("reductor")) preferences.putFloat("reductor", 9.7);
  if (!preferences.isKey("stepsize")) preferences.putFloat("stepsize", 1.8);

  // Тепер читаємо актуальні значення (вони вже точно є в пам'яті)
  int pulseWidth = preferences.getInt("pulsewidth");
  int maxSpeed = preferences.getInt("maxspeed");
  int acceleration = preferences.getInt("acceleration");
  float microstep = preferences.getFloat("microstep");
  float reductor = preferences.getFloat("reductor");
  float stepsize = preferences.getFloat("stepsize");

  // Застосовуємо налаштування до об'єкта stepper
  stepper.setMinPulseWidth(pulseWidth);
  stepper.setMaxSpeed(maxSpeed);
  stepper.setAcceleration(acceleration);
  
  // Розраховуємо коефіцієнт
  stepper_ratio = (microstep * reductor) / stepsize;

  preferences.end();
}

void moveDegrees(float degrees) {
  stepper.move(degrees * stepper_ratio);
}

void moveDegreesBlocking(float degrees) {
  moveDegrees(degrees);
  while (stepper.distanceToGo() != 0) stepper.run();
}

void moveToHomeBlocking() {
  stepper.moveTo(0);
  while (stepper.distanceToGo() != 0) stepper.run();
}

void updateCompassMode() {
  if (!compassMode) return;
  unsigned long now = millis();
  if (now - previousMillis < interval) return;
  previousMillis = now;

  updatePRY();
  float diff = azimuth - currentYaw;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  moveDegrees(diff);
}