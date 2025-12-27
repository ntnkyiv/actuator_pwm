#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "SerialProtocol.h"
#include "StepperControl.h"
#include "Compass.h"
#include "LinearActuator.h"


void handleSerialCommands() {
  if (Serial.available() == 0) return;

  String jsonString = Serial.readStringUntil('\n');
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
      serializeJson(doc, Serial);
      Serial.println();        
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
      serializeJson(doc, Serial);
      Serial.println();        
    }
    //{"cmd":"setcurrent","value":0}
    else if (strcmp(cmd, "setcurrent") == 0){
      stepper.setCurrentPosition(value);
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"absolute","value":1000}
    else if (strcmp(cmd, "absolute") == 0){
      stepper.moveTo(value);
      serializeJson(doc, Serial);
      Serial.println();
    }
      //{"cmd":"relative","value":1000}
    else if (strcmp(cmd, "relative") == 0){
      stepper.move(value);
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"degree","value":5}
    else if (strcmp(cmd, "degree") == 0){
      moveDegrees(value);
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"stop","value":0}
    else if (strcmp(cmd, "stop") == 0){
      stepper.stop();
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"calibrate","value":0}
//    else if (strcmp(cmd, "calibrate") == 0) {
//      if (!compassFound) {
//        doc["error"] = "ICM-20948 не підключено";
//      } else {
//        calibrateCompass();
//        doc["status"] = "calibration_started";
//      }    
//    }
    //{"cmd":"compassmode","value":180}; {"cmd":"compassmode","value":null}
    else if (strcmp(cmd, "compassmode") == 0){
      if (!compassFound) {
        doc["error"] = "ICM-20948 не підключено";
        serializeJson(doc, Serial); Serial.println();
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
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"azimuth","value":180}; {"cmd":"azimuth","value":null}
    else if (strcmp(cmd, "azimuth") == 0){
      if (!compassFound) {
        doc["error"] = "ICM-20948 не підключено";
        serializeJson(doc, Serial); Serial.println();
        return;
      }
      if (doc["value"].isNull()){
        doc["value"] = azimuth;
      }
      else{
        azimuth = fmod(fmod(value, 360.0) + 360.0, 360.0 );    
      }
      serializeJson(doc, Serial);
      Serial.println();
      updatePRY();
     if (abs(azimuth - currentYaw) > 180) {
        stepper.move((heading - azimuth - 180) * stepper_ratio);
      } 
      else {
      stepper.move((azimuth - heading) * stepper_ratio);
      }
    }
    //{"cmd":"getheading","value":0}
//    else if (strcmp(cmd, "getheading") == 0){
//      if (!compassFound) {
//        doc["error"] = "ICM-20948 не підключено";
//        serializeJson(doc, Serial); Serial.println();
//        return;
//      }
//      doc["value"] = getHeading();
//      serializeJson(doc, Serial);
//      Serial.println();
//    }
    //{"cmd":"pry","value":0}
    else if (strcmp(cmd, "pry") == 0){
      if (!compassFound) {
        doc["error"] = "ICM-20948 не підключено";
        serializeJson(doc, Serial); Serial.println();
        return;
      }
      updatePRY();
      doc["pitch"] = currentPitch;
      doc["roll"]  = currentRoll;
      doc["yaw"]   = currentYaw;      
      serializeJson(doc, Serial);
      Serial.println();
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
      serializeJson(doc, Serial);
      Serial.println();
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
      serializeJson(doc, Serial);
      Serial.println();
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
      serializeJson(doc, Serial);
      Serial.println();
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
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"wifi","value":null}; {"cmd":"wifi","value":0,"wifi_ssid":"your_SSID","password":"your_PASSWORD"}
    else if (strcmp(cmd, "wifi") == 0){
        if(doc["value"].isNull()){
        preferences.begin("compass", true);
        doc["wifi_ssid"] = preferences.getString("wifi_ssid", "your_SSID");
        doc["wifi_password"] = preferences.getString("wifi_password", "your_PASSWORD");
        preferences.end();
      }
      else{
        const char* new_ssid = doc["wifi_ssid"];
        const char* new_password = doc["wifi_password"];
        preferences.begin("compass", false);
        preferences.putString("wifi_ssid", new_ssid);
        preferences.putString("wifi_password", new_password);
        preferences.end();
      }
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"restart","value":0}
    else if (strcmp(cmd, "restart") == 0){
      serializeJson(doc, Serial);
      Serial.println();
      ESP.restart();
    }
   //{"cmd":"lextend","value":0}
    else if (strcmp(cmd, "lextend") == 0){
      linearExtend();
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"lretract","value":0}
    else if (strcmp(cmd, "lretract") == 0){
      linearRetract();
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"lsleep","value":0}
    else if (strcmp(cmd, "lsleep") == 0){
      linearSleep();
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"lbrake","value":0}
    else if (strcmp(cmd, "lbrake") == 0){
      linearBrake();
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"lsetspeed","value":150}
    else if (strcmp(cmd, "lsetspeed") == 0){
      linearSetSpeed(value);
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"ltime","value":20000}
    else if (strcmp(cmd, "ltime") == 0) {
      if (doc["value"].isNull()) {
        doc["value"] = linearGetBrakeTime();
      } 
      else {
        linearSetBrakeTime(value);
        doc["value"] = value;
      }
      serializeJson(doc, Serial);
      Serial.println();
    }
    //{"cmd":"linear_speed","value":180}
    else if (strcmp(cmd, "linear_speed") == 0) {
      int speed = value;
      speed = constrain(speed, -255, 255);
      linearSetSpeed(speed);
      doc["speed"] = speed;
      serializeJson(doc, Serial);
      Serial.println();
    }
    else {
      doc["cmd"] = "error";
      doc["value"] = "unknown_cmd"; 
      serializeJson(doc, Serial);
      Serial.println();
    }
  
}
