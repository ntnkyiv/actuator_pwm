#pragma once
void stepperInit();
void loadStepperSettings();
void saveStepperSetting(const char* key, float value);
void moveDegrees(float degrees);
void moveDegreesBlocking(float degrees);
void moveToHomeBlocking();
void updateCompassMode();   // викликається в loop()
