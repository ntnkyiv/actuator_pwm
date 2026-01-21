#include "Compass.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include <Preferences.h>
#include "StepperControl.h"
#include "LinearActuator.h"

extern AccelStepper stepper;
extern float stepper_ratio; 

Adafruit_ICM20948 icm;
bool compassFound = false;

float currentPitch = 0.0f;
float currentRoll  = 0.0f;
float currentYaw   = 0.0f;

float mag_off_x = 0.0f;
float mag_off_y = 0.0f;
float mag_off_z = 0.0f;

// === НИЗЬКОРІВНЕВІ ФУНКЦІЇ I2C ===

void writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.read();
}

// === ЧИТАННЯ МАГНІТОМЕТРА (MANUAL SINGLE SHOT) ===
// Повертає true, якщо дані успішно прочитані
bool readMagnetometer(float &mx, float &my, float &mz) {
  // 1. Запит на ОДНЕ вимірювання
  writeReg(AK_ADDR, AK_CNTL2, 0x01); 

  // 2. Чекаємо ~15 мс, АЛЕ крутимо мотор, щоб він не дьоргався!
  unsigned long start = millis();
  while (millis() - start < 15) {
    stepper.run(); // Важливо: тримаємо двигун живим
  }

  // 3. Перевіряємо готовність (ST1)
  uint8_t st1 = readReg(AK_ADDR, AK_ST1);
  if (st1 & 0x01) {
    // 4. Читаємо пакет (HXL...ST2)
    Wire.beginTransmission(AK_ADDR);
    Wire.write(AK_HXL);
    Wire.endTransmission(false);
    Wire.requestFrom(AK_ADDR, (uint8_t)7);

    if (Wire.available() >= 7) {
      uint8_t xL = Wire.read();
      uint8_t xH = Wire.read();
      uint8_t yL = Wire.read();
      uint8_t yH = Wire.read();
      uint8_t zL = Wire.read();
      uint8_t zH = Wire.read();
      uint8_t st2 = Wire.read(); // "Магічне" читання для розблокування

      // Склеюємо байти
      int16_t raw_x = (xH << 8) | xL;
      int16_t raw_y = (yH << 8) | yL;
      int16_t raw_z = (zH << 8) | zL;

      // Перевірка на переповнення (HOFL bit)
      if (st2 & 0x08) return false; 

      mx = (float)raw_x;
      my = (float)raw_y;
      mz = (float)raw_z;
      return true;
    }
  }
  return false;
}

// === ІНІЦІАЛІЗАЦІЯ ===
void compassInit() {
  Wire.end(); 
  Wire.begin(5, 6, 100000); 
  Wire.setTimeOut(100);
  delay(100); 

  Serial.println("Пошук ICM-20948...");

  // 1. СПОЧАТКУ запускаємо бібліотеку (вона скине чіп)
  bool icmStarted = false;
  for (int i = 0; i < 5; i++) {
    if (icm.begin_I2C(ICM_ADDR)) {
      icmStarted = true;
      Serial.println("ICM-20948 (Accel/Gyro) OK.");
      
      icm.setAccelRange(ICM20948_ACCEL_RANGE_4_G);
      icm.setGyroRange(ICM20948_GYRO_RANGE_250_DPS);
      break; 
    }
    delay(100); 
  }

  if (!icmStarted) {
    Serial.println("ПОМИЛКА: Головний чіп не відповідає.");
    compassFound = false;
    return;
  }

  // 2. ТЕПЕР примусово вмикаємо Bypass (поверх налаштувань бібліотеки)
  Serial.println("Re-enabling Bypass manually...");
  
  writeReg(ICM_ADDR, REG_BANK_SEL, 0x00);   // Bank 0
  delay(10);
  writeReg(ICM_ADDR, REG_USER_CTRL, 0x00);  // Вимикаємо I2C Master (щоб звільнити піни)
  delay(10);
  writeReg(ICM_ADDR, REG_INT_PIN_CFG, 0x02); // Вмикаємо Bypass Pin (зв'язок з магнітометром)
  delay(10);

  // 3. Перевіряємо магнітометр (тепер він має бути доступний)
  uint8_t magID = readReg(AK_ADDR, AK_WIA2);
  Serial.printf("Mag Device ID: 0x%02X (очікуємо 0x09)\n", magID);

  if (magID == 0x09) {
    compassFound = true;
    Serial.println("AK09916 Magnetometer OK!");
    
    // Скидаємо сам магнітометр
    writeReg(AK_ADDR, AK_CNTL3, 0x01); 
    delay(100);
  } else {
    Serial.println("ПОМИЛКА: Магнітометр недоступний (ID != 0x09)");
    compassFound = false;
  }

  loadCalibration();
}

// === ГОЛОВНА ФУНКЦІЯ ОНОВЛЕННЯ ===
void updatePRY() {
  // Навіть якщо компас не знайдено, спробуємо оновити акселерометр
  sensors_event_t a, g, m_dummy, temp;
  icm.getEvent(&a, &g, &temp, &m_dummy); // m_dummy ігноруємо

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float mx = 0, my = 0, mz = 0;
  
  // Читаємо магнітометр вручну
  if (compassFound) {
    if (!readMagnetometer(mx, my, mz)) {
      // Якщо читання не вдалося, використовуємо попередні значення або нулі
      // (тут можна додати логіку повторних спроб)
    }
  }

  // Застосовуємо калібрування
  mx -= mag_off_x;
  my -= mag_off_y;
  mz -= mag_off_z;

  // Pitch & Roll
  float pitch = atan2(ax, sqrt(ay * ay + az * az));
  float roll = atan2(-ay, az);

  currentPitch = pitch * 180.0f / PI;
  currentRoll  = roll  * 180.0f / PI;

  // Tilt Compensation
  float cosRoll = cos(roll);
  float sinRoll = sin(roll);
  float cosPitch = cos(pitch);
  float sinPitch = sin(pitch);

  float Xh = mx * cosPitch + mz * sinPitch;
  float Yh = mx * sinRoll * sinPitch + my * cosRoll - mz * sinRoll * cosPitch;

  float heading = atan2(Yh, Xh) * 180.0f / PI;

  if (heading < 0) heading += 360;
  if (heading >= 360) heading -= 360;

  currentPitch  = roundf(currentPitch * 10) / 10.0f;
  currentRoll   = roundf(currentRoll  * 10) / 10.0f;
  currentYaw    = roundf(heading      * 10) / 10.0f; 
}

// === КАЛІБРУВАННЯ ===
// Використовує ту саму ручну логіку читання

void waitStepperCalib() {
  while (stepper.distanceToGo() != 0) {
    stepper.run();
    yield();
    delay(100);
  }
}

void runLinearCalib(int speed, int timeMs) {
  linearSetSpeed(speed); 
  unsigned long start = millis();
  while (millis() - start < timeMs) {
    yield();
  }
  linearSetSpeed(0);
}

void runAutoCalibration() {
  Serial.println("=== СТАРТ КАЛІБРУВАННЯ ===");
  if (stepper_ratio <= 0) return;

  float min_x = 10000, max_x = -10000;
  float min_y = 10000, max_y = -10000;
  float min_z = 10000, max_z = -10000;

  long linearTimeout = 20000; 
  long steps180 = (long)(stepper_ratio * 180.0);
  long steps5deg = (long)(stepper_ratio * 5.0); 

  Serial.println("1. Нахил вниз...");
  runLinearCalib(-255, linearTimeout);
  delay(500);

  Serial.println("2. Поворот на -180...");
  stepper.move(-steps180);
  waitStepperCalib();

  Serial.println("3. Сканування 360...");
  for (int i = 0; i < 72; i++) {
    stepper.move(steps5deg);
    waitStepperCalib();
    
    // Читаємо магнітометр вручну
    float x, y, z;
    if (readMagnetometer(x, y, z)) {
       if (x < min_x) min_x = x; if (x > max_x) max_x = x;
       if (y < min_y) min_y = y; if (y > max_y) max_y = y;
       if (z < min_z) min_z = z; if (z > max_z) max_z = z;
    }
  }

  Serial.println("4. Нахил вгору...");
  runLinearCalib(255, linearTimeout);
  delay(500);

  Serial.println("5. Сканування назад 360...");
  for (int i = 0; i < 72; i++) {
    stepper.move(-steps5deg);
    waitStepperCalib();

    float x, y, z;
    if (readMagnetometer(x, y, z)) {
       if (x < min_x) min_x = x; if (x > max_x) max_x = x;
       if (y < min_y) min_y = y; if (y > max_y) max_y = y;
       if (z < min_z) min_z = z; if (z > max_z) max_z = z;
    }
  }

  Serial.println("6. Повернення в центр...");
  stepper.move(steps180);
  waitStepperCalib();

  mag_off_x = (max_x + min_x) / 2.0;
  mag_off_y = (max_y + min_y) / 2.0;
  mag_off_z = (max_z + min_z) / 2.0;

  Serial.printf("Offset Result: X=%.2f, Y=%.2f, Z=%.2f\n", mag_off_x, mag_off_y, mag_off_z);
  saveCalibration();
  
  runLinearCalib(-255, linearTimeout / 2);
  Serial.println("Калібрування завершено.");
}

void loadCalibration() {
  Preferences prefs;
  prefs.begin("compass", true);
  mag_off_x = prefs.getFloat("off_x", 0.0f);
  mag_off_y = prefs.getFloat("off_y", 0.0f);
  mag_off_z = prefs.getFloat("off_z", 0.0f);
  prefs.end();
}

void saveCalibration() {
  Preferences prefs;
  prefs.begin("compass", false);
  prefs.putFloat("off_x", mag_off_x);
  prefs.putFloat("off_y", mag_off_y);
  prefs.putFloat("off_z", mag_off_z);
  prefs.end();
}