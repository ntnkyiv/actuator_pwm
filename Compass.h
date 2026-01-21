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

// Глобальні змінні
extern Adafruit_ICM20948 icm;
extern bool compassFound;
extern float currentPitch;
extern float currentRoll;
extern float currentYaw;

// Зміщення калібрування
extern float mag_off_x;
extern float mag_off_y;
extern float mag_off_z;

// Функції
void compassInit();
void updatePRY();
void loadCalibration();
void saveCalibration();
void runAutoCalibration(); // Повне калібрування