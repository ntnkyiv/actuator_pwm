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
AsyncWebSocket ws("/ws");          // шлях /ws для WebSocket
DNSServer dnsServer;
extern bool apMode;

extern char wifi_ssid[64];
extern char wifi_password[64];
extern const char* ap_ssid = "CompassActuator";
extern const char* ap_password = "12345678";
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

//      if (cmd == "calibrate") {
//        if (!compassFound) {
//          client->text("ERROR: LSM303 не підключено");
//          return;
//        } 
//      calibrateCompass();
//      }

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
bool connectWiFi() { /* той самий код, що й раніше */ }

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
}

void wifiSetup() {
  if (strlen(wifi_ssid) > 0 && connectWiFi()) {
    apMode = false;
    Serial.print("STA IP: "); Serial.println(WiFi.localIP());
  } else {
    startAPMode();
  }

  setupRoutes();
  server.begin();
  Serial.println("Async WebServer запущено");
}

void wifiLoop() {
  if (apMode) dnsServer.processNextRequest();
  ws.cleanupClients();     // важливо! очищає відключені клієнти
}

void sendPRYJson(String json) {
  ws.textAll(json);
}