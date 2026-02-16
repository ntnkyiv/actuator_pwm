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

bool apMode = false;

float stepper_ratio = 0.0f;
float azimuth = 0.0f;
float heading = 0.0f;
bool compassMode = false;
const float G = 16384.0;
bool shouldCalibrate = false; // Прапорець для запуску калібрування

unsigned long previousMillis = 0;
const long interval = 100;
bool calibrationInProgress = false;
unsigned long lastCompassUpdate = 0;
const int compassInterval = 100; // Інтервал читання компаса (мс)

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

  // 2. Скидання CH9120
  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);
  delay(1000);
  digitalWrite(ETH_RESET_PIN, HIGH);
  delay(500);

  // 3. Ініціалізація Serial1 для зв'язку з CH9120
  // Швидкість має бути ТАКА Ж, як налаштована в конфігурації CH9120 (за замовчуванням 9600 або 115200)
  Serial1.setRxBufferSize(2048);
  Serial1.begin(115200, SERIAL_8N1, ETH_RX_PIN, ETH_TX_PIN);
  Serial.println(F("Ethernet Bridge Ready. Listening to Serial1..."));

  compassInit();
  stepperInit();
  linearInit();

  // Світлодіод
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);  // Вимкнено на старті

  Serial.println("=== ВСЕ ГОТОВО ===");
}

void loop() {
  // 1. ОНОВЛЕННЯ ПРОШИВКИ (OTA)
  if (isUpdating) {
    FWUpdateLoop(); 
    return; // Якщо оновлюємось, більше нічого не робимо
  }

  // 2. ДВИГУН (Найвищий пріоритет - має викликатися завжди)
  stepper.run();
  
  // 3. WIFI та Web (Підтримка зв'язку)
  wifiLoop();

  // 4. ОБРОБКА КОМАНД (Serial/Ethernet)
  handleSerialCommands(); 
  
  // 5. ЛІНІЙНИЙ АКТУАТОР
  linearAutoBrake();
  updateCompassMode(); 

  // 6. ОНОВЛЕННЯ ДАНИХ СЕНСОРІВ (Розумна логіка)
  // Читаємо компас ТІЛЬКИ якщо двигун стоїть на місці (distanceToGo == 0)
  // Це гарантує, що I2C не блокує генерацію кроків двигуна.
  
  if (stepper.distanceToGo() == 0 && !calibrationInProgress && !shouldCalibrate) {
    if (millis() - lastCompassUpdate > compassInterval) {
      updatePRY(); // Читаємо компас/температуру
      lastCompassUpdate = millis();
    }
  }

  // 7. КАЛІБРУВАННЯ (запуск за прапорцем)
  if (shouldCalibrate) {
    shouldCalibrate = false;
    Serial.println("Веб-запит: Старт калібрування...");
    runAutoCalibration(); 
  }

  // 8. СВІТЛОДІОД
  unsigned long currentMillis = millis();
  if (currentMillis - ledPreviousMillis >= ledInterval) {
    ledPreviousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  }
}