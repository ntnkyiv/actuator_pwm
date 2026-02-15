#include "Compass.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include <Preferences.h>
#include "StepperControl.h"
#include "LinearActuator.h"

// === PIN SETUP ===
#define STEPPER_ENABLE_PIN D10 // 0 = Enable, 1 = Disable

extern AccelStepper stepper;
extern float stepper_ratio; 

Adafruit_ICM20948 icm;

// Глобальні змінні стану
bool compassFound = false;
String compassLog = ""; 

float currentPitch = 0.0f;
float currentRoll  = 0.0f;
float currentYaw   = 0.0f;
float currentTemp  = 0.0f;

float mag_off_x = 0.0f;
float mag_off_y = 0.0f;
float mag_off_z = 0.0f;

// Буферизація
int smooth_window = 10;            
PRY_Rad pry_buffer[MAX_BUFFER_SIZE]; 
int buffer_idx = 0;                
int buffer_count = 0;              

// === НИЗЬКОРІВНЕВІ ФУНКЦІЇ ===

void logStep(String msg) {
  Serial.println(msg);
  compassLog += msg + "\n";
}

void writeReg8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg8(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.read();
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
  }
  
  pinMode(sdaPin, OUTPUT); digitalWrite(sdaPin, LOW);
  pinMode(sclPin, OUTPUT); digitalWrite(sclPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(sdaPin, HIGH);
  pinMode(sdaPin, INPUT_PULLUP);
}

// === ПРЕ-ІНІЦІАЛІЗАЦІЯ (Manual Kick) ===
bool forceMagReset(uint8_t icmAddr) {
  logStep(">>> PRE-BOOT MAG RESET <<<");

  writeReg8(icmAddr, 0x06, 0x01); // PWR_MGMT_1 -> Auto Clock
  delay(10);

  writeReg8(icmAddr, 0x7F, 0x00); 
  writeReg8(icmAddr, 0x0F, 0x02); // INT_PIN_CFG -> Bypass EN
  delay(50);

  uint8_t magAddr = 0x0C;
  uint8_t wia = readReg8(magAddr, 0x01); 

  char buff[32];
  sprintf(buff, "Mag Raw ID: 0x%02X", wia);
  logStep(String(buff));

  writeReg8(magAddr, 0x32, 0x01); // Soft Reset
  logStep("Sent Mag Soft-Reset...");
  delay(100); 

  writeReg8(icmAddr, 0x0F, 0x00); // Bypass OFF
  delay(10);

  return (wia == 0x09);
}

// === ОКРЕМА ФУНКЦІЯ ЧИТАННЯ ===
bool readMagnetometer(float &mx, float &my, float &mz) {
  sensors_event_t a, g, temp, m;
  // Один виклик для отримання всього пакету даних
  if (!icm.getEvent(&a, &g, &temp, &m)) return false;
  
  mx = m.magnetic.x;
  my = m.magnetic.y;
  mz = m.magnetic.z;
  
  // Перевірка на "завислі" нулі
  if (mx == 0.0f && my == 0.0f && mz == 0.0f) return false;
  return true; 
}

// === ГОЛОВНА ІНІЦІАЛІЗАЦІЯ (З ЦИКЛОМ СПРОБ) ===
void compassInit() {
  compassLog = "";
  logStep("=== INIT START (Retry Mode) ===");
  
  pinMode(STEPPER_ENABLE_PIN, OUTPUT);
  digitalWrite(STEPPER_ENABLE_PIN, HIGH); // Disable Motor
  delay(500); 

  recoverI2C(5, 6);
  
  Wire.end();
  Wire.begin(5, 6, 100000); 
  Wire.setTimeOut(200);
  delay(100);

  compassFound = false;
  uint8_t icmAddr = 0;

  // === ЦИКЛ СПРОБ (5 разів) ===
  for (int attempt = 1; attempt <= 5; attempt++) {
    logStep("Attempt " + String(attempt) + "/5...");

    // 1. Пошук адреси
    icmAddr = 0;
    Wire.beginTransmission(0x69);
    if (Wire.endTransmission() == 0) icmAddr = 0x69;
    else {
      Wire.beginTransmission(0x68);
      if (Wire.endTransmission() == 0) icmAddr = 0x68;
    }

    if (icmAddr != 0) {
      logStep("ICM found at 0x" + String(icmAddr, HEX));
      
      // 2. Ручне скидання
      forceMagReset(icmAddr);

      // 3. Старт бібліотеки
      if (icm.begin_I2C(icmAddr)) {
        logStep("Library Init: SUCCESS");
        
        icm.setAccelRange(ICM20948_ACCEL_RANGE_4_G);
        icm.setGyroRange(ICM20948_GYRO_RANGE_250_DPS);
        
        compassFound = true;
        loadCalibration();
        Wire.setClock(400000); 
        logStep(">>> ALL SYSTEMS GREEN");
        
        // Успіх! Виходимо з циклу
        break; 
      } else {
        logStep("Library Init: FAILED");
      }
    } else {
      logStep("ICM Accel not found!");
    }

    // Якщо не вийшло і це не остання спроба - чекаємо
    if (!compassFound && attempt < 5) {
      logStep("Waiting 3s before retry...");
      delay(3000); 
      
      // Спробуємо відновити шину ще раз
      recoverI2C(5, 6);
      Wire.begin(5, 6, 100000);
    }
  }

  if (!compassFound) {
    logStep("CRITICAL: Init Failed after 5 attempts.");
  }

  digitalWrite(STEPPER_ENABLE_PIN, LOW); 
}


// === ОНОВЛЕННЯ ДАНИХ ===
void updatePRY() {
  sensors_event_t a, g, temp, m;
  
  // 1. Зчитуємо ВСІ сенсори за один раз - це найважливіше для стабільності
  if (!icm.getEvent(&a, &g, &temp, &m)) return;

  currentTemp = temp.temperature;

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  // 2. Беремо дані магнітометра з того ж пакету
  float mx = m.magnetic.x;
  float my = m.magnetic.y;
  float mz = m.magnetic.z;

  // Якщо магнітометр відвалився (видає нулі), не оновлюємо Yaw
  if (mx == 0.0f && my == 0.0f && mz == 0.0f) {
    // Можна додати logStep для діагностики, але не часто
    return;
  }

  // 3. Застосовуємо офсети калібрування
  mx -= mag_off_x;
  my -= mag_off_y;
  mz -= mag_off_z;

  // 4. Розрахунок кутів
  float raw_p = atan2(ax, sqrt(ay*ay + az*az));
  float raw_r = atan2(-ay, az);
  
  float cp = cos(raw_p); 
  float sp = sin(raw_p);
  float cr = cos(raw_r); 
  float sr = sin(raw_r);
  
  // Компенсація нахилу (Tilt Compensation)
  float Xh = mx * cp + mz * sp;
  float Yh = mx * sr * sp + my * cr - mz * sr * cp;
  
  float yaw_raw = atan2(Yh, Xh);
  
  // 5. Фільтрація (Згладжування)
  pry_buffer[buffer_idx].p = raw_p;
  pry_buffer[buffer_idx].r = raw_r;
  pry_buffer[buffer_idx].y = yaw_raw;
  
  buffer_idx++;
  if (buffer_idx >= smooth_window) buffer_idx = 0;
  if (buffer_count < smooth_window) buffer_count++;

  float sum_sin_p = 0, sum_cos_p = 0;
  float sum_sin_r = 0, sum_cos_r = 0;
  float sum_sin_y = 0, sum_cos_y = 0;

  for (int i = 0; i < buffer_count; i++) {
    sum_sin_p += sin(pry_buffer[i].p); sum_cos_p += cos(pry_buffer[i].p);
    sum_sin_r += sin(pry_buffer[i].r); sum_cos_r += cos(pry_buffer[i].r);
    sum_sin_y += sin(pry_buffer[i].y); sum_cos_y += cos(pry_buffer[i].y);
  }

  currentPitch = atan2(sum_sin_p, sum_cos_p) * 180.0f / PI;
  currentRoll  = atan2(sum_sin_r, sum_cos_r) * 180.0f / PI;
  float tempYaw = atan2(sum_sin_y, sum_cos_y) * 180.0f / PI;

  if (tempYaw < 0) tempYaw += 360.0f;
  if (tempYaw >= 360) tempYaw -= 360.0f;
  
  currentYaw = tempYaw;

  // Округлення до 1 знаку
  currentPitch  = roundf(currentPitch * 10) / 10.0f;
  currentRoll   = roundf(currentRoll  * 10) / 10.0f;
  currentYaw    = roundf(currentYaw   * 10) / 10.0f;  
}

// === ЗБЕРЕЖЕННЯ / ЗАВАНТАЖЕННЯ ===
void loadCalibration() {
  Preferences prefs;
  prefs.begin("compass", true);
  mag_off_x = prefs.getFloat("off_x", 0.0f);
  mag_off_y = prefs.getFloat("off_y", 0.0f);
  mag_off_z = prefs.getFloat("off_z", 0.0f);
  smooth_window = prefs.getInt("smooth", 10);
  if (smooth_window < 1) smooth_window = 1;
  prefs.end();
  logStep("Calib Loaded.");
}
void saveCalibration() {
  Preferences prefs;
  prefs.begin("compass", false);
  prefs.putFloat("off_x", mag_off_x);
  prefs.putFloat("off_y", mag_off_y);
  prefs.putFloat("off_z", mag_off_z);
  prefs.end();
}
void resetCalibration() {
  mag_off_x = 0; mag_off_y = 0; mag_off_z = 0; saveCalibration();
}
void resetSmoothingBuffer() { buffer_idx = 0; buffer_count = 0; }

// === КАЛІБРУВАННЯ ТА КЕРУВАННЯ ===
void waitStepperCalib() {
  while (stepper.distanceToGo() != 0) { stepper.run(); yield(); }
}
void runLinearCalib(int speed, int timeMs) {
  linearSetSpeed(speed); 
  unsigned long start = millis();
  while (millis() - start < timeMs) { yield(); }
  linearSetSpeed(0);
}

void runAutoCalibration() {
  Serial.println("=== СТАРТ КАЛІБРУВАННЯ ===");
  if (stepper_ratio <= 0) return;

  float min_x = 10000, max_x = -10000;
  float min_y = 10000, max_y = -10000;
  float min_z = 10000, max_z = -10000;

  long linearTimeout = brakeDelayMs; 
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
    delay(200);
    
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
    delay(200);

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

extern float azimuth; 
void moveToAzimuth(float target) {
  if (!compassFound) return;
  target = fmod(fmod(target, 360.0) + 360.0, 360.0);
  azimuth = target; 
  float diff = target - currentYaw;
  if (diff > 180.0f)  diff -= 360.0f;
  if (diff < -180.0f) diff += 360.0f;
  stepper.move((long)(-diff * stepper_ratio));
}

void setMotorState(bool enabled) {
  digitalWrite(STEPPER_ENABLE_PIN, enabled ? LOW : HIGH);
}