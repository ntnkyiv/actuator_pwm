#pragma once
void linearInit();
void linearExtend();
void linearRetract();
void linearSleep();
void linearBrake();
void linearToggle();
void linearAutoBrake();
void linearSetSpeed(int speed);

void linearSetBrakeTime(uint32_t ms);   // встановити час у мс
uint32_t linearGetBrakeTime();          // отримати поточний час

extern int linearSpeed;
extern bool linearExtended;