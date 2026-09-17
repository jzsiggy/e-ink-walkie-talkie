/*
 * E-Ink Messenger — firmware (Supabase, hardcoded WiFi)
 * Hardware: Adafruit ESP32 Feather V2 + 2.9" Grayscale ThinkInk FeatherWing
 *           + 1.3" SSD1306 OLED (0x3D, STEMMA QT) + CardKB (0x5F)
 *
 * Backend: users(id, first_name, ...) + messages(id, sender, content, created_at).
 * See supabase.h for the two endpoints used.
 *
 * Behavior contract:
 *  - Timer wake  -> narrated poll on the OLED; keyboard is never read, so
 *    input is inherently blocked; screen dark = asleep = button works.
 *  - Button wake -> compose session; polling cannot start mid-typing (the
 *    sleep timer only exists while asleep). Only fetch is after a send.
 *  - Any failed refresh (no WiFi or server error) redraws the e-ink once
 *    with "disconnected..." top-right; a cold boot with nothing cached shows
 *    the "No Signal" screen instead. Cleared on the next successful fetch.
 *
 * Files:
 *   config.h            pins, ids, sizes, timing, e-ink layout
 *   secrets.h           WiFi + Supabase credentials (gitignored; copy secrets.h.example)
 *   state.h/.cpp        message cache kept in RTC memory across deep sleep
 *   net.h/.cpp          WiFi up/down
 *   supabase.h/.cpp     fetchLatest() / postMessage()
 *   eink.h/.cpp         drawEink() with pixel-measured word wrap
 *   ui.h/.cpp           OLED + CardKB on the switched STEMMA QT rail
 *   modes.h/.cpp        composeLoop() / pollSequence()
 *
 * Libraries: Adafruit EPD, Adafruit GFX, Adafruit SSD1306, ArduinoJson (v7)
 * Board: "Adafruit Feather ESP32 V2", upload speed 115200.
 *   arduino-cli compile --fqbn esp32:esp32:adafruit_feather_esp32_v2 .
 *   arduino-cli upload  --fqbn esp32:esp32:adafruit_feather_esp32_v2:UploadSpeed=115200 -p <port> .
 */

#include "driver/rtc_io.h"
#include "config.h"
#include "net.h"
#include "ui.h"
#include "modes.h"

static void goToSleep() {
  peripheralsOff();                                // OLED + keyboard off; screen dark
  wifiOff();
  rtc_gpio_pullup_en((gpio_num_t)WAKE_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)WAKE_BTN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BTN, 0);   // press = LOW
  esp_sleep_enable_timer_wakeup(POLL_MIN * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  pinMode(QT_POWER, OUTPUT);
  digitalWrite(QT_POWER, LOW);
  pinMode(WAKE_BTN, INPUT_PULLUP);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    composeLoop();                                 // button wake: typing session
  } else {
    pollSequence(cause == ESP_SLEEP_WAKEUP_UNDEFINED);   // timer wake or first power-on
  }

  goToSleep();                                     // one job done -> sleep
}

void loop() { /* never reached */ }
