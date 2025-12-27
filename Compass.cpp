// Compass.cpp — ICM-20948 замість LSM303
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_ICM20X.h>
#include "Compass.h"
#include "WiFiManager.h"

Adafruit_ICM20948 icm;
sensors_event_t accel, gyro, mag, temp;

void compassInit() {
  // Пробуємо дві можливі адреси
  if (!icm.begin_I2C(0x69) && !icm.begin_I2C(0x68)) {
    Serial.println("ICM-20948 не знайдено!");
    compassFound = false;
    return;
  }

  // Налаштування
  icm.setAccelRange(ICM20948_ACCEL_RANGE_4_G);
  icm.setGyroRange(ICM20948_GYRO_RANGE_2000_DPS);
  icm.setMagDataRate(AK09916_MAG_DATARATE_100_HZ);

  compassFound = true;
  Serial.println("ICM-20948 ініціалізовано (9-DOF)");
}

void updatePRY() {
  if (!compassFound) return;

  sensors_event_t a, g, m, temp;
  icm.getEvent(&a, &g, &temp, &m);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float mx = m.magnetic.x;
  float my = m.magnetic.y;
  float mz = m.magnetic.z;

  // === ВИЗНАЧАЄМО, ЯКА ВІСЬ ВКАЗУЄ ВНИЗ ===
  // (найбільше від’ємне прискорення ≈ -9.8 м/с²)
  float abs_x = fabs(ax);
  float abs_y = fabs(ay);
  float abs_z = fabs(az);

  float heading;

  if (abs_z > abs_x && abs_z > abs_y) {
    // Z-вниз (найпоширеніший випадок — плата горизонтально)
    currentPitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0f / PI;
    currentRoll  = atan2(-ay, az) * 180.0f / PI;
    heading = atan2(my, mx);                     // класичний випадок
  }
  else if (abs_x > abs_y) {
    // X-вниз (чіп вертикально, вісь X вниз)
    currentPitch = atan2(-ay, sqrt(ax*ax + az*az)) * 180.0f / PI;
    currentRoll  = atan2(az, ax) * 180.0f / PI;
    heading = atan2(mz, my);                     // міняємо осі
  }
  else {
    // Y-вниз (чіп вертикально, вісь Y вниз)
    currentPitch = atan2(ay, sqrt(ax*ax + az*az)) * 180.0f / PI;
    currentRoll  = atan2(-ax, ay) * 180.0f / PI;
    heading = atan2(-mx, mz);                    // міняємо осі
  }

  heading = heading * 180.0f / PI;
  if (heading < 0) heading += 360;
  currentYaw = heading;

  // Округлення до однієї десятої
  currentPitch = roundf(currentPitch * 10) / 10.0f;
  currentRoll  = roundf(currentRoll  * 10) / 10.0f;
  currentYaw   = roundf(currentYaw   * 10) / 10.0f;
}