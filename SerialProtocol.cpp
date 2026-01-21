#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "SerialProtocol.h"
#include "StepperControl.h"
#include "Compass.h"
#include "LinearActuator.h"
#include "fw_update.h"
#include "WiFiManager.h"

String receivedMD5 = "";


void handleSerialCommands() {
  if (Serial1.available() == 0) return;

  String jsonString = Serial1.readStringUntil('\n');
  jsonString.trim();
  if (jsonString.length() == 0) return;

  StaticJsonDocument<300> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    doc["error"] = "json_parse";
    serializeJson(doc, Serial);
    Serial.println();
    return;
  }

  const char* cmd = doc["cmd"] | "";
  float value = doc["value"] | 0;

  // === КРОКОВИЙ ДВИГУН ===
    //{"cmd":"maxspeed","value":5000}; {"cmd":"maxspeed","value":null}
    if (strcmp(cmd, "maxspeed") == 0){
      if (doc["value"].isNull()){
        preferences.begin("compass", true);
        doc["value"] = preferences.getInt("maxspeed", 1000);
        preferences.end();
      }
      else{
        stepper.setMaxSpeed(value);
        preferences.begin("compass", false);
        preferences.putInt("maxspeed", value);
        preferences.end();
      }
      serializeJson(doc, Serial1);
      Serial1.println();        
    }
    //{"cmd":"acceleration","value":5000}; {"cmd":"acceleration","value":null}
    else if (strcmp(cmd, "acceleration") == 0){
      if (doc["value"].isNull()){
        preferences.begin("compass", true);
        doc["value"] = preferences.getInt("acceleration", 1000);
        preferences.end();
      }
      else{
        stepper.setAcceleration(value);
        preferences.begin("compass", false);
        preferences.putInt("acceleration", value);
        preferences.end();
      }
      serializeJson(doc, Serial1);
      Serial1.println();        
    }
    //{"cmd":"pulsewidth","value":20}; {"cmd":"pulsewidth","value":null}
    else if (strcmp(cmd, "pulsewidth") == 0){
      if(doc["value"].isNull()){
        preferences.begin("compass", true);
        doc["value"] = preferences.getInt("pulsewidth", 20);
        preferences.end();
      }
      else{
        stepper.setMinPulseWidth(value);
        preferences.begin("compass", false);
        preferences.putInt("pulsewidth", value);
        preferences.end();
      }
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"microstep","value":32}; {"cmd":"microstep","value":null}
    else if (strcmp(cmd, "microstep") == 0){
      if(doc["value"].isNull()){
        preferences.begin("compass", true);
        doc["value"] = preferences.getFloat("microstep", 32);
        preferences.end();
      }
      else{
        preferences.begin("compass", false);
        preferences.putFloat("microstep", value);
        loadStepperSettings();
        preferences.end();
      }
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"reductor","value":9.7}; {"cmd":"reductor","value":null}
    else if (strcmp(cmd, "reductor") == 0){
      preferences.begin("compass", doc["value"].isNull() ? true : false); // true для читання, false для запису
      if(doc["value"].isNull()){
        float savedValue = preferences.getFloat("reductor", 9.7);
        doc["value"] = savedValue;
      }
      else{
        float newValue = doc["value"].as<float>();
        preferences.putFloat("reductor", newValue);
        loadStepperSettings();
      }
      preferences.end();
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"stepsize","value":1.8}; {"cmd":"stepsize","value":null}
    else if (strcmp(cmd, "stepsize") == 0){
      if(doc["value"].isNull()){
        preferences.begin("compass", true);
        float val = preferences.getFloat("stepsize", 1.8);
        preferences.end();
        doc["value"] = val; // Перезапис
      }
      else {
        float val = doc["value"].as<float>();
        preferences.begin("compass", false);
        preferences.putFloat("stepsize", val);
        loadStepperSettings();
        preferences.end();
      }
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"setcurrent","value":null}
    else if (strcmp(cmd, "setcurrent") == 0){
      stepper.setCurrentPosition(value);
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"absolute","value":1000}
    else if (strcmp(cmd, "absolute") == 0){
      stepper.moveTo(value);
      serializeJson(doc, Serial1);
      Serial1.println();
    }
      //{"cmd":"relative","value":1000}
    else if (strcmp(cmd, "relative") == 0){
      stepper.move(value);
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"degree","value":5}
    else if (strcmp(cmd, "degree") == 0){
      moveDegrees(value);
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"stop","value":0}
    else if (strcmp(cmd, "stop") == 0){
      stepper.stop();
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    else if (strcmp(cmd, "compassmode") == 0){
      if (!compassFound) {
        doc["error"] = "ICM-20948 не підключено";
        serializeJson(doc, Serial1); 
        Serial1.println();
        return;
      }
      if (doc["value"].isNull()){
        compassMode = false;
        doc["value"] = "off";
      }
      else{
        compassMode = true;
        azimuth = fmod(fmod(value, 360.0) + 360.0, 360.0 );    
      }
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"azimuth","value":180}; {"cmd":"azimuth","value":null}
    else if (strcmp(cmd, "azimuth") == 0){
      if (!compassFound) {
        doc["error"] = "ICM-20948 не підключено";
        serializeJson(doc, Serial1); 
        Serial1.println();
        return;
      }
      if (doc["value"].isNull()){
        doc["value"] = azimuth;
      }
      else{
        azimuth = fmod(fmod(value, 360.0) + 360.0, 360.0 );    
      }
      serializeJson(doc, Serial1);
      Serial1.println();
      updatePRY();
     if (abs(azimuth - currentYaw) > 180) {
        stepper.move((heading - azimuth - 180) * stepper_ratio);
      } 
      else {
      stepper.move((azimuth - heading) * stepper_ratio);
      }
  // === Компас, Акселерометр ===
    }
    //{"cmd":"pry","value":null}
    else if (strcmp(cmd, "pry") == 0){
      if (!compassFound) {
        doc["error"] = "ICM-20948 не підключено";
        serializeJson(doc, Serial1); 
        Serial1.println();
        return;
      }
      doc["pitch"] = currentPitch;
      doc["roll"]  = currentRoll;
      doc["yaw"]   = currentYaw;      
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"calibrate"}
    else if (strcmp(cmd, "calibrate") == 0) {
    // 1. Повідомляємо сервер, що почали
    doc.clear();
    doc["cmd"] = "calibrate";
    doc["status"] = "started";
    serializeJson(doc, Serial1);
    Serial1.println();

    // 2. ЗАПУСКАЄМО ПРОЦЕС (блокуючий)
    runAutoCalibration();

    // 3. Повідомляємо результат
    doc.clear();
    doc["cmd"] = "calibrate";
    doc["status"] = "done";
    doc["off_x"] = mag_off_x;
    doc["off_y"] = mag_off_y;
    doc["off_z"] = mag_off_z;
    serializeJson(doc, Serial1);
    Serial1.println();
    }

  // === WIFI ===
  //{"cmd":"wifi_status"}
    else if (strcmp(cmd, "wifi_status") == 0) {
    // Викликаємо функцію, яка перевірить стан і відправить JSON
      printCurrentWiFiStatus(); 
    }    
  // {"cmd":"wifimode", "value":1} (Увімкнути) / {"cmd":"wifimode", "value":0} (Вимкнути)
    else if (strcmp(cmd, "wifimode") == 0) {
      bool enable = (value == 1);
      
      preferences.begin("compass", false);
      preferences.putBool("wifi_enabled", enable);
      preferences.end();

      if (enable) {
        wifiSetup(); // Спробує підключитися або створить AP
        doc["status"] = "wifi_enabled";
      } else {
        stopWiFi(); // Повністю вимкне радіо
        doc["status"] = "wifi_disabled";
      }
      
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"wifi","value":null}; {"cmd":"wifi","wifi_ssid":"...","wifi_password":"..."}
    else if (strcmp(cmd, "wifi") == 0){
        if(doc["value"].isNull()){
        preferences.begin("compass", true);
        doc["wifi_ssid"] = preferences.getString("wifi_ssid", "your_SSID");
        doc["wifi_password"] = preferences.getString("wifi_password", "your_PASSWORD");
        doc["wifi_enabled"] = preferences.getBool("wifi_enabled", true); // Додаємо статус у відповідь
        preferences.end();
      }
      else{
        const char* new_ssid = doc["wifi_ssid"];
        const char* new_password = doc["wifi_password"];
        preferences.begin("compass", false);
        preferences.putString("wifi_ssid", new_ssid);
        preferences.putString("wifi_password", new_password);
        preferences.end();
        
        // Опціонально: можна одразу спробувати перепідключитися
        stopWiFi();
        wifiSetup();
      }
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"ap_config","value":null}; {"cmd":"ap_config","ap_ssid":"...","ap_password":"..."}
    else if (strcmp(cmd, "ap_config") == 0) {
    if (doc["ap_ssid"].isNull()) {
      // Читання поточних налаштувань
      preferences.begin("compass", true);
      doc["ap_ssid"] = preferences.getString("ap_ssid", "CompassActuator");
      doc["ap_password"] = preferences.getString("ap_password", "12345678");
      preferences.end();
    } 
    else {
      // Запис нових налаштувань
      const char* new_ap_ssid = doc["ap_ssid"];
      const char* new_ap_pass = doc["ap_password"];
      
      preferences.begin("compass", false);
      preferences.putString("ap_ssid", new_ap_ssid);
      preferences.putString("ap_password", new_ap_pass);
      preferences.end();
      
      // Оновлюємо глобальні змінні в пам'яті, щоб не треба було перезавантажувати для відображення
      // (але для застосування змін WiFi все одно краще зробити restart)
      strncpy(ap_ssid, new_ap_ssid, sizeof(ap_ssid));
      strncpy(ap_password, new_ap_pass, sizeof(ap_password));
      
      doc["status"] = "saved_restart_required";
    }
    serializeJson(doc, Serial1);
    Serial1.println();
  }

  // === Лінійний актуатор ===

   //{"cmd":"lextend","value":null}
    else if (strcmp(cmd, "lextend") == 0){
      linearExtend();
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"lretract","value":null}
    else if (strcmp(cmd, "lretract") == 0){
      linearRetract();
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"lsleep","value":null}
    else if (strcmp(cmd, "lsleep") == 0){
      linearSleep();
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"lbrake","value":null}
    else if (strcmp(cmd, "lbrake") == 0){
      linearBrake();
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"lsetspeed","value":150}
    else if (strcmp(cmd, "lsetspeed") == 0){
      linearSetSpeed(value);
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"ltime","value":20000}; {"cmd":"ltime","value":null} 
    else if (strcmp(cmd, "ltime") == 0) {
      if (doc["value"].isNull()) {
        doc["value"] = linearGetBrakeTime();
      } 
      else {
        linearSetBrakeTime(value);
        doc["value"] = value;
      }
      serializeJson(doc, Serial1);
      Serial1.println();
    }
    //{"cmd":"restart","value":null}
    else if (strcmp(cmd, "restart") == 0){
      serializeJson(doc, Serial1);
      Serial1.println();
      ESP.restart();
    }
    //{"cmd":"ota_start","size":<filesize>}
    else if (doc["cmd"] == "ota_start") {
      fullFileSize = doc["size"];
      receivedMD5 = doc["md5"].as<String>();
      receivedMD5.toLowerCase(); // Обов'язково!
      receivedMD5.trim();

      Update.abort(); // Скидаємо все старе
      if (Update.begin(fullFileSize)) {
        if (!Update.setMD5(receivedMD5.c_str())) {
             doc["error"] = "md5_set_failed";
             serializeJson(doc, Serial1);
             Serial1.println();
             return;
        }
        // 1. Чекаємо трохи і вичищаємо ВСЕ, що залетіло в буфер випадково
        while(Serial1.available()) { Serial1.read(); } 

        isUpdating = true;
        bytesReceived = 0;
        bufferIdx = 0;
        packetsSaved = 0;
        lastUpdateByteTime = millis();
        Serial.printf("[OTA] Старт. Очікую %u байт. MD5: %s\n", fullFileSize, receivedMD5.c_str());
        Serial.flush();     
        // 2. ВІДПРАВЛЯЄМО СИГНАЛ СКРИПТУ (тільки зараз!)
        Serial1.println("RES:READY"); 
      }
    }
    // Запит: {"cmd":"version"}
    else if (strcmp(cmd, "version") == 0) {
      doc["fw_version"] = FW_VERSION;
      
      // Автоматичні макроси часу компіляції
      // Це дозволяє бачити, коли саме був натиснутий кнопку "Compile"
      String buildTime = String(__DATE__) + " " + String(__TIME__);
      doc["build_date"] = buildTime;
      
      doc["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX); // Унікальний ID чіпа (опціонально)

      serializeJson(doc, Serial1);
      Serial1.println();
    }
    else {
      doc["cmd"] = "error";
      doc["value"] = "unknown_cmd"; 
      serializeJson(doc, Serial1);
      Serial1.println();
    }
}
