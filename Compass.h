#pragma once
#include <Arduino.h>
#include <Adafruit_ICM20948.h>

// === НАЛАШТУВАННЯ АДРЕС ТА РЕГІСТРІВ ===
#define ICM_ADDR 0x68
#define AK_ADDR  0x0C 

// Регістри ICM-20948
#define REG_BANK_SEL    0x7F
#define REG_PWR_MGMT_1  0x06
#define REG_INT_PIN_CFG 0x0F
#define REG_USER_CTRL   0x03

// Регістри AK09916 (Магнітометр)
#define AK_WIA2         0x01 
#define AK_ST1          0x10 
#define AK_HXL          0x11 
#define AK_CNTL2        0x31 
#define AK_CNTL3        0x32 

// === НАЛАШТУВАННЯ БУФЕРА ===
#define MAX_BUFFER_SIZE 50 // Максимальна глибина історії

// Структура для зберігання однієї точки в радіанах
struct PRY_Rad {
  float p;
  float r;
  float y;
};

// Глобальні змінні буфера
extern int smooth_window;     // Поточний розмір вікна (напр. 10)
extern PRY_Rad pry_buffer[MAX_BUFFER_SIZE]; // Масив історії
extern int buffer_idx;        // Поточна позиція запису (голова)
extern int buffer_count;      // Скільки реальних даних є в буфері

// Глобальні змінні
extern Adafruit_ICM20948 icm;
extern bool compassFound;
extern String compassLog;
extern float currentPitch;
extern float currentRoll;
extern float currentYaw;
extern float azimuth;
extern float currentTemp;

// Зміщення калібрування
extern float mag_off_x;
extern float mag_off_y;
extern float mag_off_z;

// Змінні emi фільтра
extern float filterAlpha;

// Функції
void compassInit();
void updatePRY();
void loadCalibration();
void saveCalibration();
void runAutoCalibration(); // Повне калібрування
void moveToAzimuth(float target);
void resetCalibration();
void setMotorState(bool enabled); // true = увімкнено (тримай), false = вимкнено (вільно)
void saveFilterSettings();