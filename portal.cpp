#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "net.h"
#include "ui.h"
#include "portal.h"
#include "portal_html.h"

static WebServer     server(80);
static DNSServer     dns;
static String        rowsHtml;        // nearby networks, rendered once per scan
static String        notice;          // one-shot banner at the top of the page
static PortalResult  result;
static bool          done;
static unsigned long lastActivity;

/* ------------------------------- helpers -------------------------------- */

static bool escPressed() { return readKey() == 0x1B; }

static void showSetupScreen() {
  String join = String("Join: ") + PORTAL_SSID;
  oledMsg("WiFi setup", join.c_str(), "then open 192.168.4.1");
}

/* Blocking scan (~2 s). Strongest first, duplicates removed. */
static void scanNetworks() {
  int n = WiFi.scanNetworks();
  int order[32];
  int count = 0;
  for (int i = 0; i < n && count < 32; i++) {
    if (WiFi.SSID(i).length() == 0) continue;
    bool dup = false;
    for (int j = 0; j < count; j++) if (WiFi.SSID(order[j]) == WiFi.SSID(i)) { dup = true; break; }
    if (!dup) order[count++] = i;
  }
  for (int a = 0; a < count; a++)                 // insertion sort by RSSI, strongest first
    for (int b = a; b > 0 && WiFi.RSSI(order[b]) > WiFi.RSSI(order[b - 1]); b--) {
      int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
    }
  rowsHtml = "";
  for (int k = 0; k < count; k++) {
    int i = order[k];
    appendNetworkRow(rowsHtml, htmlEscape(WiFi.SSID(i)), signalBars(WiFi.RSSI(i)),
                     WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  WiFi.scanDelete();
}

static void sendPage() {
  String cur, pw;
  bool configured = wifiLoad(cur, pw);
  String curEsc = htmlEscape(cur);
  String page;
  page.reserve(4096 + rowsHtml.length());
  buildSetupPage(page, PORTAL_SSID, notice, rowsHtml, configured ? curEsc.c_str() : nullptr);
  notice = "";
  server.send(200, "text/html", page);
}

static void sendDone(const String& title, const char* sub) {
  String page;
  page.reserve(4096);
  buildDonePage(page, PORTAL_SSID, title.c_str(), sub);
  server.send(200, "text/html", page);
}

/* ------------------------------- handlers ------------------------------- */

static void handleRoot() {
  lastActivity = millis();
  if (server.hasArg("scan")) scanNetworks();
  sendPage();
}

static void handleSave() {
  lastActivity = millis();
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (ssid.length() == 0) { notice = "Enter a network name."; sendPage(); return; }

  oledMsg("Testing WiFi...", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());                   // STA side; the AP stays up
  unsigned long t0 = millis();
  bool cancelled = false;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < PORTAL_TEST_MS) {
    dns.processNextRequest();
    if (escPressed()) { cancelled = true; break; }
    delay(50);
  }
  if (cancelled) {
    WiFi.disconnect(false);                                 // drop the attempt; nothing is saved
    server.send(200, "text/plain", "Cancelled on the device.");
    result = PORTAL_CANCELLED;
    done = true;
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiSave(ssid, pass);
    oledMsg("WiFi saved!");
    sendDone("Connected to " + htmlEscape(ssid), "Saved. The messenger will use this network from now on.");
    result = PORTAL_SAVED;
    done = true;
  } else {
    WiFi.disconnect(false);                                 // drop the STA attempt, keep the AP
    notice = "Could not connect to \xE2\x80\x9C" + htmlEscape(ssid) + "\xE2\x80\x9D. Check the password and try again.";
    oledMsg("Wrong password?", "Try again");
    sendPage();
    delay(1500);
    showSetupScreen();
  }
}

static void handleForget() {
  lastActivity = millis();
  String cur, pw;
  wifiLoad(cur, pw);
  wifiForget();
  oledMsg("WiFi forgotten");
  sendDone("Forgot " + htmlEscape(cur), "The messenger will show the setup hint until a network is saved.");
  result = PORTAL_FORGOTTEN;
  done = true;
}

static void handleRedirect() {                              // captive-portal probes and anything unknown
  lastActivity = millis();
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

/* --------------------------------- run ---------------------------------- */

PortalResult portalRun() {
  result = PORTAL_TIMEOUT;
  done = false;
  notice = "";

  WiFi.mode(WIFI_AP_STA);                                   // STA needed for scanning and the test connect
  WiFi.disconnect(false);
  oledMsg("WiFi setup", "Scanning...");
  scanNetworks();

  WiFi.softAP(PORTAL_SSID);                                 // open network, default 192.168.4.1
  dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/forget", HTTP_POST, handleForget);
  server.on("/generate_204", handleRedirect);               // Android
  server.on("/hotspot-detect.html", handleRedirect);        // iOS / macOS
  server.on("/connecttest.txt", handleRedirect);            // Windows
  server.on("/ncsi.txt", handleRedirect);
  server.on("/fwlink", handleRedirect);
  server.onNotFound(handleRedirect);
  server.begin();

  showSetupScreen();
  lastActivity = millis();
  bool hadStation = false;
  while (!done) {
    dns.processNextRequest();
    server.handleClient();
    if (escPressed()) { result = PORTAL_CANCELLED; break; }
    bool station = WiFi.softAPgetStationNum() > 0;
    if (station) lastActivity = millis();
    if (station != hadStation) {
      hadStation = station;
      if (station) oledMsg("Phone connected", "Open 192.168.4.1");
      else         showSetupScreen();
    }
    if (millis() - lastActivity > PORTAL_TIMEOUT_MS) break;
    delay(5);
  }

  delay(300);                                               // let the last response flush
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  wifiOff();
  return result;
}
