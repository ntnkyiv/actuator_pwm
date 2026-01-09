#include "fw_update.h"

// Визначаємо змінні тут
bool isUpdating = false;
size_t fullFileSize = 0;
size_t bytesReceived = 0;


void FWUpdateLoop() {
  if (isUpdating) {
    // РЕЖИМ ОНОВЛЕННЯ
    while (Serial1.available()) {
      uint8_t b = Serial1.read();
      if (Update.write(&b, 1) != 1) {
        Update.printError(Serial);
        isUpdating = false;
        return;
      }
      bytesReceived++;
      // Перевірка завершення
      if (bytesReceived >= fullFileSize) {
        if (Update.end(true)) {
          Serial.println("Оновлення успішне! Перезавантаження...");
          delay(1000);
          ESP.restart();
        } else {
          Update.printError(Serial);
          isUpdating = false;
        }
      }
    }  
  }
}