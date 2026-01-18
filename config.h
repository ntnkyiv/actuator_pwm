#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include <Preferences.h>

#define FW_VERSION "1.0.0"

// === Піни ===
#define STEP_PIN  3   // GPIO для STEP
#define DIR_PIN   4  // GPIO для DIR

// === Глобальні змінні ===

extern bool apMode;
extern float stepper_ratio;
extern float azimuth;
extern float heading;
extern bool compassMode;

extern AccelStepper stepper;
extern Preferences preferences;

extern float currentPitch;
extern float currentRoll;
extern float currentYaw;   // азимут з магнітометра
extern bool compassFound;
extern const float G;
extern bool calibrationInProgress;

// === Налаштування компас-режиму ===
extern const long interval;
extern unsigned long previousMillis;

