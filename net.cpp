#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "net.h"

bool wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (unsigned long i = 0; i < WIFI_TIMEOUT_MS / 100 && WiFi.status() != WL_CONNECTED; i++) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
