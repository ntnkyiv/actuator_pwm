#include "fw_update.h"

// Визначаємо змінні тут
extern String receivedMD5;
bool isUpdating = false;
size_t fullFileSize = 0;
size_t bytesReceived = 0;
uint32_t lastUpdateByteTime = 0;

uint8_t chunkBuffer[BufferSize]; 
int bufferIdx = 0;
int packetsSaved = 0;

void FWUpdateLoop() {
  if (!isUpdating) return;

  if (millis() - lastUpdateByteTime > 10000) {
    Serial.println("[OTA] Timeout! Скасування...");
    Update.abort(); // Скасувати процес у бібліотеці
    isUpdating = false;
    return;
  }

  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    lastUpdateByteTime = millis(); // Оновлюємо таймер при отриманні байта

    // СИНХРОНІЗАЦІЯ: Ігноруємо нулі на самому початку
    if (bytesReceived == 0 && b != 0xE9) {
      continue;
    }

    chunkBuffer[bufferIdx++] = b;
    bytesReceived++;

    if (bufferIdx == BufferSize || (bytesReceived == fullFileSize && bufferIdx > 0)) {
      size_t written = Update.write(chunkBuffer, bufferIdx);
      if (written == bufferIdx) {
        packetsSaved++;
        // 2. ВІДПРАВЛЯЄМО ПІДТВЕРДЖЕННЯ ТІЛЬКИ ПІСЛЯ УСПІШНОГО ЗАПИСУ
        // Це дає сигнал Python надсилати наступну порцію
        Serial1.printf("RES:OK_PKT_%d\n", packetsSaved);
        Serial1.flush(); // Примусово виштовхуємо в UART
      } 
      else {
        Serial.printf("[OTA] Помилка запису Flash! %u != %u\n", written, bufferIdx);        
        Serial1.println("RES:ERR_WRITE");
        isUpdating = false;
        return;
      }
      bufferIdx = 0;
    }

    // 6. ЗАВЕРШЕННЯ ТА ПЕРЕВІРКА MD5
    if (bytesReceived >= fullFileSize) {
      size_t expected = fullFileSize;
      size_t actual = Update.progress(); // Скільки реально прийняла бібліотека
    
      Serial.printf("[OTA] Фінал: Очікували %u, Записали %u\n", expected, actual);

      if (Update.end(true)) { 
        Serial.println("[OTA] Успіх! MD5 збігся. Перезавантаження...");
        Serial1.println("RES:DONE");
        delay(500);
        ESP.restart();
      } 
      else {
        Serial.print("[OTA] Помилка Update.end(): ");
        Update.printError(Serial); // Тут напише "MD5 Failed" або "Aborted"
        
        // Діагностика для розуміння причини
        if (Update.getError() == UPDATE_ERROR_MD5) {
            Serial1.println("RES:MD5_ERR");
        } else {
            Serial1.println("RES:ERR_FINALIZE");
        }
        isUpdating = false;
      }
      return;
    }
  }
}
