#pragma once
void linearInit();
void linearExtend();
void linearRetract();
void linearSleep();
void linearBrake();
void linearToggle();
void linearAutoBrake();
void linearSetSpeed(int speed);

void linearSetBrakeTime(uint32_t ms);
uint32_t linearGetBrakeTime();

void linearSetMinSpeed(uint8_t val);
uint8_t linearGetMinSpeed();
int linearLevelToSpeed(int level); // рівень 1–5 → PWM

extern int linearSpeed;
extern bool linearExtended;
extern uint32_t brakeDelayMs;
extern uint8_t linearMinSpeed;