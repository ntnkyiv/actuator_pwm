// fw_update.h
#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#include <Arduino.h>
#include <Update.h>

#define BufferSize 512

// Глобальні змінні стану (доступні всюди)
extern bool isUpdating;
extern size_t fullFileSize;
extern size_t bytesReceived;
extern uint32_t lastUpdateByteTime;

extern uint8_t chunkBuffer[BufferSize]; 
extern int bufferIdx;
extern int packetsSaved;

// Прототипи функцій
void FWUpdateLoop();
void startMD5();
void updateMD5(uint8_t* data, size_t len);
void finalizeMD5();

#endif