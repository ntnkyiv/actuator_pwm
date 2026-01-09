// fw_update.h
#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#include <Arduino.h>
#include <Update.h>

// Глобальні змінні стану (доступні всюди)
extern bool isUpdating;
extern size_t fullFileSize;
extern size_t bytesReceived;

// Прототипи функцій
void FWUpdateLoop();

#endif