#include <Arduino.h>
#include "WiFiManager.h"
#include "config.h"
#include "WebPages.h"
#include "LinearActuator.h"
#include "StepperControl.h"
#include <DNSServer.h>
#include "Compass.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

// Нові об'єкти
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
extern bool apMode;

char wifi_ssid[64];
char wifi_password[64];
char ap_ssid[64] = "CompassActuator";
char ap_password[64] = "12345678";
extern void sendPRY();

// ─────────────────────── WebSocket події ───────────────────────
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      data[len] = 0;
      String cmd = (char*)data;
      cmd.trim();

      Serial.println("Web команда: " + cmd);

      if (cmd.startsWith("degree:")) {
        float deg = cmd.substring(7).toFloat();
        moveDegrees(deg);
      }

      else if (cmd.startsWith("azimuth:")) {
        if (!compassFound) {
          client->text("ERROR: ICM-20948 не підключено");
          return;
        }
        float az = cmd.substring(8).toFloat();
        azimuth = fmod(fmod(az, 360.0) + 360.0, 360.0);
        float diff = azimuth - currentYaw;
        if (diff > 180) diff -= 360;
        if (diff < -180) diff += 360;
        moveDegrees(diff);
      }

      else if (cmd == "pry" || cmd == "heading") {
        if (!compassFound) {
          client->text("ERROR: LSM303 не підключено");
          return;
        }
        safeSendPRY();
      }

      else if (cmd.startsWith("linear_speed:")) {
        int speed = cmd.substring(13).toInt();
        linearSetSpeed(constrain(speed, -255, 255));
      }      
      else if (cmd == "extend")   linearExtend();
      else if (cmd == "retract")  linearRetract();
      else if (cmd == "brake")    linearBrake();
      else if (cmd == "sleep")    linearSleep();
      else if (cmd == "linear_toggle") linearToggle();
      else if (cmd == "stop") {
        stepper.stop();
        stepper.runToPosition();  // миттєва зупинка
      }

      client->text("OK");
    }
  }
}

void safeSendPRY() {
  if (calibrationInProgress) {
    Serial.println("PRY заблоковано під час калібрування");
    return;
  }
  updatePRY();
  StaticJsonDocument<200> doc;
  doc["pitch"] = currentPitch;
  doc["roll"]  = currentRoll;
  doc["yaw"]   = currentYaw;

  String json;
  serializeJson(doc, json);
  ws.textAll(json);}

// ─────────────────────── Налаштування роутів ───────────────────────
void setupRoutes() {
  extern void sendPRY();  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", wifi_config_html);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("s", true) && request->hasParam("p", true)) {
      request->getParam("s", true)->value().toCharArray(wifi_ssid, sizeof(wifi_ssid));
      request->getParam("p", true)->value().toCharArray(wifi_password, sizeof(wifi_password));

      preferences.begin("compass", false);
      preferences.putString("wifi_ssid", wifi_ssid);
      preferences.putString("wifi_password", wifi_password);
      preferences.end();

      request->send(200, "text/html", "<h1>Збережено. Перезавантаження...</h1>"
      "<script>setTimeout(()=>location='/',3000);</script>");
      delay(1000);
      ESP.restart();
    }
  });

  // Captive Portal — всі запити перенаправляємо на головну
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (apMode) request->redirect("/");
    else request->send(404, "text/plain", "Not found");
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
}

// ─────────────────────── Основні функції ───────────────────────
void stopWiFi() {
  WiFi.disconnect(true);  // Відключитися і стерти конфіг
  WiFi.mode(WIFI_OFF);    // Вимкнути радіомодуль
  Serial.println("WiFi вимкнено (режим енергозбереження)");
}

// Спроба підключення до WiFi
bool connectWiFi() {
  preferences.begin("compass", true); // Тільки читання
  String ssid = preferences.getString("wifi_ssid", "");
  String pass = preferences.getString("wifi_password", "");
  preferences.end();

  if (ssid.length() == 0) {
    Serial.println("SSID не задано в налаштуваннях.");
    return false;
  }

  // Оновлюємо глобальні змінні для відображення в Web
  ssid.toCharArray(wifi_ssid, sizeof(wifi_ssid));
  pass.toCharArray(wifi_password, sizeof(wifi_password));

  Serial.print("Спроба підключення до: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Чекаємо ~10 сек
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi підключено!");
    return true;
  } else {
    Serial.println("Не вдалося підключитися до WiFi.");
    return false;
  }
}

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  
  // Використовуємо змінні ap_ssid та ap_password
  WiFi.softAP(ap_ssid, ap_password);
  
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  Serial.print("AP Mode запущено. SSID: "); Serial.print(ap_ssid);
  Serial.print(" IP: "); Serial.println(WiFi.softAPIP());
}

void wifiSetup() {
  preferences.begin("compass", true);
  bool enabled = preferences.getBool("wifi_enabled", true);
  
  // 2. ЗАВАНТАЖУЄМО налаштування AP з пам'яті
  String saved_ap_ssid = preferences.getString("ap_ssid", "CompassActuator");
  String saved_ap_pass = preferences.getString("ap_password", "12345678");
  
  // Копіюємо в глобальні масиви
  saved_ap_ssid.toCharArray(ap_ssid, sizeof(ap_ssid));
  saved_ap_pass.toCharArray(ap_password, sizeof(ap_password));
  
  preferences.end();

  if (!enabled) {
    stopWiFi();
    return;
  }

  // Спроба підключення до роутера
  if (connectWiFi()) {
    apMode = false;
    Serial.print("STA IP: "); Serial.println(WiFi.localIP());
    Serial.println("WiFi запущено");
  } else {
    Serial.println("Перехід в режим точки доступу (AP)...");
    startAPMode();
  }

  setupRoutes();
  server.begin();
  Serial.println("Async WebServer запущено");
}

void wifiLoop() {
  // Якщо WiFi вимкнено глобально, нічого не робимо
  if (WiFi.getMode() == WIFI_OFF) return;

  if (apMode) dnsServer.processNextRequest();
  ws.cleanupClients();
}

void sendPRYJson(String json) {
  // Відправляємо дані тільки якщо WiFi активний
  if (WiFi.getMode() != WIFI_OFF) {
    ws.textAll(json);
  }
}