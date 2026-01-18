#pragma once
void wifiSetup();
void wifiLoop();
void stopWiFi();
extern void updateAndSendPRY();
void sendPRYJson(String json);
void safeSendPRY();
void printCurrentWiFiStatus();

extern char wifi_ssid[64];
extern char wifi_password[64];
extern char ap_ssid[64];
extern char ap_password[64];