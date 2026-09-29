#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"
#include "net.h"

static const char* NVS_NS = "wifi";

bool wifiLoad(String& ssid, String& pass) {
  Preferences p;
  if (!p.begin(NVS_NS, true)) return false;       // namespace absent = never configured
  ssid = p.getString("ssid", "");
  pass = p.getString("pass", "");
  p.end();
  return ssid.length() > 0;
}

bool wifiConfigured() {
  String s, pw;
  return wifiLoad(s, pw);
}

void wifiSave(const String& ssid, const String& pass) {
  Preferences p;
  p.begin(NVS_NS, false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
}

void wifiForget() {
  Preferences p;
  p.begin(NVS_NS, false);
  p.clear();
  p.end();
}

bool wifiConnect() {
  String ssid, pass;
  if (!wifiLoad(ssid, pass)) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (unsigned long i = 0; i < WIFI_TIMEOUT_MS / 100 && WiFi.status() != WL_CONNECTED; i++) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
