/* WiFi station + credential storage (NVS namespace "wifi"). */
#pragma once
#include <Arduino.h>

bool wifiConfigured();                                   // an SSID is stored
bool wifiLoad(String& ssid, String& pass);               // false if nothing stored
void wifiSave(const String& ssid, const String& pass);   // replaces whatever was stored
void wifiForget();                                       // back to unconfigured

bool wifiConnect();   // uses stored credentials; blocks up to WIFI_TIMEOUT_MS; false if unconfigured
void wifiOff();
