#pragma once
#include <Arduino.h>

extern bool compassFound;
extern float currentPitch;
extern float currentRoll;
extern float currentYaw;

// Глобальні змінні зміщення
extern float mag_off_x;
extern float mag_off_y;
extern float mag_off_z;

void compassInit();
void updatePRY();
void loadCalibration();
void saveCalibration();
void getRawMag(float &x, float &y, float &z);

// Нова функція запуску повного циклу калібрування
void runAutoCalibration();
