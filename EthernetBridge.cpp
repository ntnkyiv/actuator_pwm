// EthernetBridge.cpp
#include "EthernetBridge.h"
#include <WiFi.h>

// CH9120 підключений до дефолтного UART0 на XIAO ESP32S3
#define ETH_RX_PIN  44   // GPIO44 → TXD CH9120
#define ETH_TX_PIN  43   // GPIO43 → RXD CH9120

// Опціонально: пін reset
#define ETH_RESET_PIN  7   // GPIO7 → RSTi CH9120 (активний LOW)

HardwareSerial SerialETH(1);          // Тепер UART1! (UART0 зарезервований під USB)
WiFiServer     tcpServer(5000);
WiFiClient     tcpClient;

void ethernetBridgeSetup() {
  // Скидання CH9120 (рекомендовано при старті)
  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);   // скидаємо
  delay(100);
  digitalWrite(ETH_RESET_PIN, HIGH);  // відпускаємо
  delay(500);                         // чекаємо ініціалізації

  // Ініціалізація UART1 на потрібних пінах
  SerialETH.begin(115200, SERIAL_8N1, ETH_RX_PIN, ETH_TX_PIN);

  tcpServer.begin();
  tcpServer.setNoDelay(true);

  Serial.println(F("\nEthernet bridge (CH9120) запущено"));
  Serial.printf("   Піни: RX=%d (CH9120 TXD), TX=%d (CH9120 RXD)\n", ETH_RX_PIN, ETH_TX_PIN);
  Serial.println(F("   Reset pin: GPIO7"));
  Serial.println(F("   TCP порт: 5000"));
  Serial.println(F("   Готовий приймати JSON-команди по Ethernet"));
}

void ethernetBridgeLoop() {
  if (!tcpClient.connected()) {
    tcpClient = tcpServer.available();
    if (tcpClient) {
      Serial.println(F("Ethernet клієнт підключився"));
    }
    return;
  }

  while (tcpClient.available()) {
    uint8_t b = tcpClient.read();
    SerialETH.write(b);
  }

  while (SerialETH.available()) {
    uint8_t b = SerialETH.read();
    tcpClient.write(b);
  }

  if (!tcpClient.connected()) {
    Serial.println(F("Ethernet клієнт відключився"));
    tcpClient.stop();
  }
}