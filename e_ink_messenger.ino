/*
 * E-Ink Messenger — firmware (Supabase, hardcoded WiFi)
 * Hardware: Adafruit ESP32 Feather V2 + 2.9" Grayscale ThinkInk FeatherWing
 *           + 1.3" SSD1306 OLED (0x3D, STEMMA QT) + CardKB (0x5F)
 *
 * Backend: users(id, first_name, ...) + messages(id, sender, content, created_at).
 * Endpoints:
 *   send: POST /rest/v1/messages       {"sender": MY_USER_ID, "content": ...}
 *   view: POST /rest/v1/rpc/latest     {"me": MY_USER_ID}
 *         -> {"outgoing": {user, message} | null, "incoming": {user, message} | null}
 *
 * Behavior contract:
 *  - Timer wake  -> narrated poll on the OLED; keyboard is never read, so
 *    input is inherently blocked; screen dark = asleep = button works.
 *  - Button wake -> compose session; polling cannot start mid-typing (the
 *    sleep timer only exists while asleep). Only fetch is after a send.
 *
 * Libraries: Adafruit EPD, Adafruit GFX, Adafruit SSD1306, ArduinoJson
 * Board: "Adafruit Feather ESP32 V2", upload speed 115200.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include "Adafruit_ThinkInk.h"
#include "driver/rtc_io.h"

/* ========================= CONFIG (per device) ========================= */
const char* WIFI_SSID   = "riverside";
const char* WIFI_PASS   = "REDACTED";
const char* SUPA_URL    = "https://znytjcfmpkeiambjjhqb.supabase.co";  // no trailing slash
const char* SUPA_KEY    = "REDACTED";
const long  MY_USER_ID  = 1;          // your row's id in the users table
const int   TZ_OFFSET   = 0;           // hours from UTC, e.g. -3, +2
const uint64_t POLL_MIN = 1;          // minutes between mailbox checks
/* ======================================================================= */

/* ---- pins: values proven during bring-up ---- */
#define EPD_CS     15
#define EPD_DC     33
#define SRAM_CS    32
#define EPD_RESET  -1
#define EPD_BUSY   -1
#define EPD_SPI    &SPI
#define QT_POWER    2      // HIGH = OLED + keyboard powered
#define WAKE_BTN   13      // wing button on the D13 position (verify by pressing)
#define OLED_ADDR  0x3D
#define KBD_ADDR   0x5F

ThinkInk_290_Grayscale4_EAAMFGN epd(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, EPD_SPI);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

/* ---- state that survives deep sleep ---- */
RTC_DATA_ATTR int64_t myMsgId    = 0;
RTC_DATA_ATTR int64_t theirMsgId = 0;
RTC_DATA_ATTR bool haveMine      = false;
RTC_DATA_ATTR bool haveTheirs    = false;
RTC_DATA_ATTR char myMsg[220]    = "";
RTC_DATA_ATTR char myTime[8]     = "";
RTC_DATA_ATTR char theirMsg[220] = "";
RTC_DATA_ATTR char theirTime[8]  = "";
RTC_DATA_ATTR char theirName[24] = "";

/* =============================== WiFi ================================= */

bool wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 100 && WiFi.status() != WL_CONNECTED; i++) delay(100);
  return WiFi.status() == WL_CONNECTED;          // ~10 s timeout
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

/* ============================= Supabase =============================== */

void addAuth(HTTPClient &http) {
  http.addHeader("apikey", SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
  http.addHeader("Content-Type", "application/json");
}

void isoToLocalHHMM(const char* iso, char* out, size_t n) {
  if (strlen(iso) < 16) { strlcpy(out, "--:--", n); return; }
  int h = (iso[11]-'0')*10 + (iso[12]-'0');
  int m = (iso[14]-'0')*10 + (iso[15]-'0');
  h = ((h + TZ_OFFSET) % 24 + 24) % 24;
  snprintf(out, n, "%02d:%02d", h, m);
}

/* One round trip: my last sent + newest from anyone else.
   Returns true if anything changed (state updated). */
bool fetchLatest() {
  WiFiClientSecure client;
  client.setInsecure();          // private toy; cert pinning is the tidy upgrade
  HTTPClient http;
  http.begin(client, String(SUPA_URL) + "/rest/v1/rpc/latest");
  addAuth(http);
  StaticJsonDocument<96> req;
  req["me"] = MY_USER_ID;
  String body;
  serializeJson(req, body);
  int code = http.POST(body);
  if (code != 200) { http.end(); return false; }

  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) return false;

  bool changed = false;

  JsonVariant outgoing = doc["outgoing"];
  if (!outgoing.isNull()) {                      // edge case: never sent anything
    int64_t id = outgoing["message"]["id"] | 0;
    if (id > myMsgId) {
      myMsgId = id;
      haveMine = true;
      strlcpy(myMsg, outgoing["message"]["content"] | "", sizeof myMsg);
      isoToLocalHHMM(outgoing["message"]["created_at"] | "", myTime, sizeof myTime);
      changed = true;
    }
  }

  JsonVariant incoming = doc["incoming"];
  if (!incoming.isNull()) {                      // edge case: nobody else wrote yet
    int64_t id = incoming["message"]["id"] | 0;
    if (id > theirMsgId) {
      theirMsgId = id;
      haveTheirs = true;
      strlcpy(theirMsg,  incoming["message"]["content"] | "", sizeof theirMsg);
      strlcpy(theirName, incoming["user"]["first_name"] | "?", sizeof theirName);
      isoToLocalHHMM(incoming["message"]["created_at"] | "", theirTime, sizeof theirTime);
      changed = true;
    }
  }

  return changed;
}

bool postMessage(const char* text) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, String(SUPA_URL) + "/rest/v1/messages");
  addAuth(http);
  http.addHeader("Prefer", "return=minimal");
  StaticJsonDocument<384> doc;
  doc["sender"] = MY_USER_ID;
  doc["content"] = text;
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  return code == 201;                            // PostgREST: 201 Created
}

/* ============================== E-ink ================================= */

/* Word wrap. Draws ".." at the end if the text didn't fit. */
void wrapPrint(Adafruit_GFX &g, const char* s, int x, int y,
               int maxChars, int maxLines, int lineH) {
  String rest = s;
  int cy = y, lines = 0;
  while (rest.length() > 0 && lines < maxLines) {
    int cut = (int)rest.length() <= maxChars ? rest.length()
                                             : rest.lastIndexOf(' ', maxChars);
    if (cut <= 0) cut = maxChars;
    bool lastLine = (lines == maxLines - 1);
    String line = rest.substring(0, cut);
    rest = rest.substring(cut);
    rest.trim();
    if (lastLine && rest.length() > 0) {         // truncated: mark it
      if ((int)line.length() > maxChars - 2) line = line.substring(0, maxChars - 2);
      line += "..";
    }
    g.setCursor(x, cy);
    g.print(line);
    cy += lineH;
    lines++;
  }
}

void drawEink() {
  epd.begin(THINKINK_GRAYSCALE4);
  epd.clearBuffer();
  epd.setTextColor(EPD_BLACK);                   // all text in full black
  epd.setTextSize(1);

  /* --- top pane, built-in font: what I last sent --- */
  epd.setFont(NULL);                             // built-in: y = top of text
  epd.setCursor(4, 4);
  if (haveMine) {
    epd.print("YOU  ");
    epd.print(myTime);
    wrapPrint(epd, myMsg, 4, 15, 48, 2, 10);
  } else {
    epd.print("YOU  (nothing sent yet)");        // edge case
  }

  epd.drawLine(0, 40, 295, 40, EPD_LIGHT);       // the ONLY light-gray element

  /* --- bottom pane: label built-in, message in FreeSans 9pt --- */
  if (haveTheirs) {
    epd.setFont(NULL);
    epd.setCursor(4, 46);
    epd.print(theirName);
    epd.print("  ");
    epd.print(theirTime);
    epd.setFont(&FreeSans9pt7b);                 // GFX font: y = BASELINE
    wrapPrint(epd, theirMsg, 4, 72, 32, 4, 16);
  } else {
    epd.setFont(&FreeSans9pt7b);                 // edge case: empty thread
    wrapPrint(epd, "No messages yet. Say hi!", 4, 72, 32, 4, 16);
  }

  epd.setFont(NULL);                             // restore default
  epd.display();
  epd.powerDown();
}

/* ========================== OLED + keyboard =========================== */

char readKey() {
  Wire.requestFrom(KBD_ADDR, 1);                 // CardKB: reads only
  return Wire.available() ? Wire.read() : 0;
}

void oledMsg(const char* line1, const char* line2 = "") {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println(line1);
  if (line2[0]) oled.println(line2);
  oled.display();
}

/* Button-wake path: the typing session. No polling can start in here —
   the 5-minute timer is only armed at sleep time. */
void composeLoop() {
  Wire.begin();
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return;
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  String buf = "";
  unsigned long lastKey = millis();
  oledMsg("Type, Enter=send", "Esc=cancel");

  while (millis() - lastKey < 60000) {           // 60 s idle -> give up (draft lost)
    char k = readKey();
    if (k == 0) { delay(20); continue; }
    lastKey = millis();

    if (k == 0x1B) return;                       // Esc = cancel

    if (k == 0x0D) {                             // Enter -> send
      if (buf.length() == 0) return;
      oledMsg("Sending...");
      if (!wifiConnect()) { oledMsg("No WiFi.", "Try again later"); delay(2500); return; }
      bool ok = postMessage(buf.c_str());
      if (ok) {
        fetchLatest();                           // my row back + anything new of hers
        wifiOff();
        oledMsg("Sent!");
        drawEink();
        delay(800);
      } else {
        wifiOff();
        oledMsg("Send failed.", "Check internet");
        delay(2500);
      }
      return;
    }

    if (k == 0x08) {                             // backspace
      if (buf.length()) buf.remove(buf.length() - 1);
    } else if (k >= 0x20 && k < 0x7F && buf.length() < 190) {
      buf += k;
    }

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println(buf);
    oled.display();
  }
}

/* ====================== Polling (timer wake) ========================== */

/* Narrated on the OLED so a watching human knows the device is busy.
   The keyboard is never read in here, so user input is inherently
   blocked until the screen goes dark and the device sleeps. */
void pollSequence(bool firstBoot) {
  digitalWrite(QT_POWER, HIGH);                  // light the OLED for status
  delay(500);
  Wire.begin();
  if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
  }
  oledMsg("Waking up...");
  delay(300);

  oledMsg("Connecting to WiFi...");
  if (!wifiConnect()) {
    oledMsg("No WiFi.", "Trying again later");
    delay(1200);
    oledMsg("Falling asleep...");
    delay(400);
    return;
  }

  oledMsg("Checking messages...");
  bool changed = fetchLatest();
  wifiOff();

  if (changed || firstBoot) {
    oledMsg("New message!", "Updating e-ink...");
    drawEink();                                  // ~2 s flash
    oledMsg("Done!");
  } else {
    oledMsg("Nothing new.");
  }
  delay(700);
  oledMsg("Falling asleep...");
  delay(400);
}

/* =============================== Sleep ================================ */

void goToSleep() {
  digitalWrite(QT_POWER, LOW);                   // OLED + keyboard off; screen dark
  wifiOff();
  rtc_gpio_pullup_en((gpio_num_t)WAKE_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)WAKE_BTN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BTN, 0);   // press = LOW
  esp_sleep_enable_timer_wakeup(POLL_MIN * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

/* =============================== Main ================================= */

void setup() {
  pinMode(QT_POWER, OUTPUT);
  digitalWrite(QT_POWER, LOW);
  pinMode(WAKE_BTN, INPUT_PULLUP);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    /* Button wake: typing session. */
    digitalWrite(QT_POWER, HIGH);
    delay(500);                                  // OLED settle time
    composeLoop();
  } else {
    /* Timer wake or first power-on: narrated poll. */
    pollSequence(cause == ESP_SLEEP_WAKEUP_UNDEFINED);
  }

  goToSleep();                                   // one job done -> sleep
}

void loop() { /* never reached */ }