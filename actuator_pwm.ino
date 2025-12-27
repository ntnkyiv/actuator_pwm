#include "config.h"
#include "Compass.h"
#include "StepperControl.h"
#include "WiFiManager.h"
#include "SerialProtocol.h"
#include "LinearActuator.h"
#include "EthernetBridge.h"
#include <Wire.h>

Preferences preferences;                                      
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

char wifi_ssid[64] = {0};
char wifi_password[64] = {0};
bool apMode = false;

float stepper_ratio = 0.0f;
float azimuth = 0.0f;
float heading = 0.0f;
bool compassMode = false;
const float G = 16384.0;

unsigned long previousMillis = 0;
const long interval = 100;
bool calibrationInProgress = false;
float currentPitch = 0.0f;
float currentRoll  = 0.0f;
float currentYaw   = 0.0f;
bool compassFound = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== CompassActuator ESP32-S3 + CH9120 ==="));
  // 1. Спочатку WiFi — найбезпечніше
  preferences.begin("compass", true);
  preferences.getString("wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
  preferences.getString("wifi_password", wifi_password, sizeof(wifi_password));
  preferences.end();
  Serial.println("NVS прочитано");

  wifiSetup();           // ← запускаємо WiFi ПЕРШИМ!
  Serial.println("WiFi запущено");
  ethernetBridgeSetup();          // Новий Ethernet міст на GPIO16/17

  // 2. Ініціалізація I2C — з таймаутом
  Serial.print("I2C init на пінах SDA=4, SCL=5 ... ");
  Wire.setClock(100000);
  Wire.begin();
  delay(100);

  // Перевіряємо, чи є LSM303
  compassInit();
  stepperInit();
  linearInit();

  Serial.println("=== ВСЕ ГОТОВО ===");
}
void loop() {
  handleSerialCommands();   // JSON по Serial
  stepper.run();            // обов'язково часто!
  linearAutoBrake();
  updateCompassMode();      // режим компаса
  //wifiLoop();               // WebServer.handleClient(), dnsServer.processNextRequest() тощо
  ethernetBridgeLoop();       // Ethernet ↔ JSON міст (головна магія)
}