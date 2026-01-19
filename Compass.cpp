#include "Compass.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include <Preferences.h>
#include "StepperControl.h"    // Додано для керування кроковиком
#include "LinearActuator.h"    // Додано для керування актуатором

// Зовнішні об'єкти (вони створені в інших файлах)
extern AccelStepper stepper;
extern float stepper_ratio; 

Adafruit_ICM20948 icm;
bool compassFound = false;

float currentPitch = 0.0f;
float currentRoll  = 0.0f;
float currentYaw   = 0.0f;

// Зміщення за замовчуванням
float mag_off_x = 0.0f;
float mag_off_y = 0.0f;
float mag_off_z = 0.0f;

// === Допоміжні функції для калібрування ===

// Чекаємо завершення руху кроковика (з yield для WiFi)
void waitStepperCalib() {
  while (stepper.distanceToGo() != 0) {
    stepper.run();
    yield(); 
  }
}

// Рух лінійного актуатора за часом
void runLinearCalib(int speed, int timeMs) {
  linearSetSpeed(speed); 
  unsigned long start = millis();
  while (millis() - start < timeMs) {
    // stepper.run(); // Можна розкоментувати, якщо треба тримати позицію кроковика силою
    yield();
  }
  linearSetSpeed(0);
}

// === ОСНОВНА ФУНКЦІЯ КАЛІБРУВАННЯ ===
void runAutoCalibration() {
  Serial.println("=== СТАРТ КАЛІБРУВАННЯ (COMPASS.CPP) ===");

  if (stepper_ratio <= 0) {
    Serial.println("ПОМИЛКА: stepper_ratio не задано!");
    return;
  }

  // Ініціалізація min/max екстремальними значеннями
  float min_x = 10000, max_x = -10000;
  float min_y = 10000, max_y = -10000;
  float min_z = 10000, max_z = -10000;

  long linearTimeout = 20000; // Час повного ходу актуатора (20 сек)
  
  // Розрахунок кроків
  long steps180 = (long)(stepper_ratio * 180.0);
  long steps5deg = (long)(stepper_ratio * 5.0); 

  // --- 1. Нахил вниз (Retract) ---
  Serial.println("1. Нахил вниз...");
  runLinearCalib(-255, linearTimeout);
  delay(500);

  // --- 2. Поворот на -180 ---
  Serial.println("2. Поворот на -180...");
  stepper.move(-steps180);
  waitStepperCalib();

  // --- 3. Сканування +360 (Нижня півсфера) ---
  Serial.println("3. Сканування 360...");
  for (int i = 0; i < 72; i++) {
    stepper.move(steps5deg);
    waitStepperCalib();
    delay(100); // Пауза для стабілізації

    float x, y, z;
    getRawMag(x, y, z);
    
    if (x < min_x) min_x = x; if (x > max_x) max_x = x;
    if (y < min_y) min_y = y; if (y > max_y) max_y = y;
    if (z < min_z) min_z = z; if (z > max_z) max_z = z;
  }

  // --- 4. Нахил вгору (Extend) ---
  Serial.println("4. Нахил вгору...");
  runLinearCalib(255, linearTimeout);
  delay(500);

  // --- 5. Сканування назад -360 (Верхня півсфера) ---
  Serial.println("5. Сканування назад 360...");
  for (int i = 0; i < 72; i++) {
    stepper.move(-steps5deg);
    waitStepperCalib();
    delay(100);

    float x, y, z;
    getRawMag(x, y, z);
    
    if (x < min_x) min_x = x; if (x > max_x) max_x = x;
    if (y < min_y) min_y = y; if (y > max_y) max_y = y;
    if (z < min_z) min_z = z; if (z > max_z) max_z = z;
  }

  // --- 6. Повернення в центр (+180) ---
  Serial.println("6. Повернення в центр...");
  stepper.move(steps180);
  waitStepperCalib();

  // --- ОБЧИСЛЕННЯ ---
  mag_off_x = (max_x + min_x) / 2.0;
  mag_off_y = (max_y + min_y) / 2.0;
  mag_off_z = (max_z + min_z) / 2.0;

  Serial.printf("Offset Result: X=%.2f, Y=%.2f, Z=%.2f\n", mag_off_x, mag_off_y, mag_off_z);

  // Збереження
  saveCalibration();
  
  // Паркування актуатора в центр
  runLinearCalib(-255, linearTimeout / 2);
  Serial.println("Калібрування завершено.");
}

// === Стандартні функції ===

void compassInit() {
  Wire.end(); 
  Wire.begin(5, 6, 100000); 
  delay(50); 
  Serial.println("Пошук ICM-20948...");

  for (int i = 0; i < 10; i++) {
    if (icm.begin_I2C(0x68)) {
      compassFound = true;
      Serial.println("ICM-20948 ЗНАЙДЕНО!");
      icm.setAccelRange(ICM20948_ACCEL_RANGE_4_G);
      icm.setGyroRange(ICM20948_GYRO_RANGE_250_DPS);
      icm.setMagDataRate(AK09916_MAG_DATARATE_100_HZ);
      
      // Завантажуємо старі дані при старті
      loadCalibration();
      return; 
    }
    Serial.print(".");
    delay(500); 
  }
  Serial.println("\nПОМИЛКА: ICM-20948 не знайдено.");
  compassFound = false;
}

void updatePRY() {
  if (!compassFound) return;

  sensors_event_t a, g, m, temp;
  icm.getEvent(&a, &g, &temp, &m);

  // 1. Сирі дані + Калібрування
  float mx = m.magnetic.x - mag_off_x;
  float my = m.magnetic.y - mag_off_y;
  float mz = m.magnetic.z - mag_off_z;

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  // 2. Pitch & Roll
  float pitch = atan2(ax, sqrt(ay * ay + az * az));
  float roll = atan2(-ay, az);

  currentPitch = pitch * 180.0f / PI;
  currentRoll  = roll  * 180.0f / PI;

  // 3. Tilt Compensation
  float cosRoll = cos(roll);
  float sinRoll = sin(roll);
  float cosPitch = cos(pitch);
  float sinPitch = sin(pitch);

  float Xh = mx * cosPitch + mz * sinPitch;
  float Yh = mx * sinRoll * sinPitch + my * cosRoll - mz * sinRoll * cosPitch;

  // 4. Heading
  float heading = atan2(Yh, Xh) * 180.0f / PI;

  if (heading < 0) heading += 360;
  if (heading >= 360) heading -= 360;

  currentPitch  = roundf(currentPitch * 10) / 10.0f;
  currentRoll   = roundf(currentRoll  * 10) / 10.0f;
  currentYaw    = roundf(heading      * 10) / 10.0f; 
}

void loadCalibration() {
  Preferences prefs;
  prefs.begin("compass", true);
  mag_off_x = prefs.getFloat("off_x", 0.0f);
  mag_off_y = prefs.getFloat("off_y", 0.0f);
  mag_off_z = prefs.getFloat("off_z", 0.0f);
  prefs.end();
  Serial.printf("Load Calib: %.2f %.2f %.2f\n", mag_off_x, mag_off_y, mag_off_z);
}

void saveCalibration() {
  Preferences prefs;
  prefs.begin("compass", false);
  prefs.putFloat("off_x", mag_off_x);
  prefs.putFloat("off_y", mag_off_y);
  prefs.putFloat("off_z", mag_off_z);
  prefs.end();
}

void getRawMag(float &x, float &y, float &z) {
  if (!compassFound) return;
  sensors_event_t a, g, m, t;
  icm.getEvent(&a, &g, &t, &m);
  x = m.magnetic.x;
  y = m.magnetic.y;
  z = m.magnetic.z;
}