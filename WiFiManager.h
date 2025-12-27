#pragma once
void wifiSetup();
void wifiLoop();
extern void updateAndSendPRY();
void sendPRYJson(String json);
void safeSendPRY();