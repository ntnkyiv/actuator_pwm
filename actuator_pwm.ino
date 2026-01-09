#include "config.h"
#include "Compass.h"
#include "StepperControl.h"
#include "WiFiManager.h"
#include "SerialProtocol.h"
#include "LinearActuator.h"
#include "fw_update.h"
#include <Wire.h>
#include <Update.h>

Preferences preferences;                                      
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// CH9120 підключений до дефолтного UART0 на XIAO ESP32S3
#define ETH_RX_PIN  44   // GPIO44 → TXD CH9120
#define ETH_TX_PIN  43   // GPIO43 → RXD CH9120
#define ETH_RESET_PIN  7   // GPIO7 → RSTi CH9120 (активний LOW)


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

//=== Світлодіод ===
unsigned long ledPreviousMillis = 0;
const long ledInterval = 500;  // Інтервал мигання в мс (тут 500 мс = 1 раз на секунду)
bool ledState = false;

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
  //ethernetBridgeSetup();          // Новий Ethernet міст на GPIO16/17

  // 2. Скидання CH9120
  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);
  delay(1000);
  digitalWrite(ETH_RESET_PIN, HIGH);
  delay(500);

  // 3. Ініціалізація Serial1 для зв'язку з CH9120
  // Швидкість має бути ТАКА Ж, як налаштована в конфігурації CH9120 (за замовчуванням 9600 або 115200)
  Serial1.begin(115200, SERIAL_8N1, ETH_RX_PIN, ETH_TX_PIN);

  Serial.println(F("Ethernet Bridge Ready. Listening to Serial1..."));


  // 4. Ініціалізація I2C — з таймаутом
  Serial.print("I2C init на пінах SDA=4, SCL=5 ... ");
  Wire.setClock(100000);
  Wire.begin();
  delay(100);

  // Перевіряємо, чи є LSM303
  compassInit();
  stepperInit();
  linearInit();

  // Світлодіод
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);  // Вимкнено на старті

  Serial.println("=== ВСЕ ГОТОВО ===");
}

void loop() {

  FWUpdateLoop();
  handleSerialCommands();   // JSON по Serial
  stepper.run();            // обов'язково часто!
  linearAutoBrake();
  updateCompassMode();      // режим компаса
  unsigned long currentMillis = millis();

  if (currentMillis - ledPreviousMillis >= ledInterval) {
    ledPreviousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  }
}