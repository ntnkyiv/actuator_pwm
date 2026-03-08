#pragma once
#include <Arduino.h>
#include <Adafruit_BNO08x.h>

extern Adafruit_BNO08x bno08x;
extern bool compassFound;
extern float currentPitch;
extern float currentRoll;
extern float currentYaw;
extern float currentTemp;
extern float azimuth;

void compassInit();
void updatePRY();
void getBNODiagnostics(uint32_t &outLastOkMs, uint32_t &outFailCount,
                       uint8_t &outI2C4A, uint8_t &outI2C4B, uint8_t &outRecoveryStage);
void runBNO08xCalibration(int cycles);
void moveToAzimuth(float target);
void setMotorState(bool enabled);
