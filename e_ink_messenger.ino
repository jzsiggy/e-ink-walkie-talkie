/*
 * E-Ink Messenger — firmware (Supabase backend, WiFi via setup hotspot)
 * Hardware: Adafruit ESP32 Feather V2 + 2.9" Grayscale ThinkInk FeatherWing
 *           + 1.3" SSD1306 OLED (0x3D, STEMMA QT) + CardKB (0x5F)
 *
 * Backend: users(id, first_name, ...) + messages(id, sender, content, created_at).
 * See supabase.h for the two endpoints used.
 *
 * Behavior contract:
 *  - WiFi credentials live in NVS, written by the setup hotspot. Nothing is
 *    compiled in. Unconfigured: e-ink shows "Hi, love!" plus a hint, the poll
 *    timer is never armed, and button C only says "No WiFi configured".
 *  - Button A (top-left) -> open AP "bluesandgreens" + captive portal for up to
 *    5 idle minutes. Saving tests the network first. Then a poll, then sleep.
 *    Esc on the keyboard cancels setup at any time; nothing is changed.
 *  - Timer wake  -> narrated poll on the OLED; keyboard is never read, so
 *    input is inherently blocked; screen dark = asleep = buttons work.
 *  - Button C (top-right) tap -> compose session; polling cannot start mid-typing
 *    (the sleep timer only exists while asleep). Only fetch is after a send.
 *    Hold it ~1.5 s instead -> poll now (same narrated sequence as a timer wake).
 *  - Any failed refresh (no WiFi or server error) redraws the e-ink once
 *    with "disconnected..." top-right; a cold boot with nothing cached shows
 *    the "No Signal" screen instead. Cleared on the next successful fetch.
 *  - Battery level (2x200k divider on GPIO 35, no fuel gauge) is shown top-right
 *    in 10% steps; a step change triggers one redraw on the next poll.
 *    At or below BATT_LOW_PCT the poll ends with three "Charge me!" flashes
 *    and the sleep timer stretches to POLL_LOW_MIN.
 *
 * Files:
 *   config.h            pins, ids, sizes, timing, e-ink layout
 *   secrets.h           Supabase credentials (gitignored; copy secrets.h.example)
 *   state.h/.cpp        message cache kept in RTC memory across deep sleep
 *   net.h/.cpp          WiFi station + credentials in NVS
 *   portal.h/.cpp       setup hotspot (AP + DNS + web page; markup in portal_html.h)
 *   supabase.h/.cpp     fetchLatest() / postMessage()
 *   textfmt.h/.cpp      UTF-8 -> ASCII shaping, local timestamps (host-testable)
 *   battery.h/.cpp      LiPo level from the ADC divider
 *   eink.h/.cpp         drawEink() with pixel-measured word wrap
 *   ui.h/.cpp           OLED + CardKB on the switched STEMMA QT rail
 *   modes.h/.cpp        composeLoop() / pollSequence() / setupLoop()
 *
 * Libraries: Adafruit EPD, Adafruit GFX, Adafruit SSD1306, ArduinoJson (v7)
 * Board: "Adafruit Feather ESP32 V2", upload speed 115200.
 *   arduino-cli compile --fqbn esp32:esp32:adafruit_feather_esp32_v2 .
 *   arduino-cli upload  --fqbn esp32:esp32:adafruit_feather_esp32_v2:UploadSpeed=115200 -p <port> .
 */

#include "driver/rtc_io.h"
#include "config.h"
#include "state.h"
#include "net.h"
#include "ui.h"
#include "modes.h"

static void goToSleep() {
  oledMsg("Falling asleep...");                    // every path ends here, so the cue is consistent
  delay(400);
  peripheralsOff();                                // OLED + keyboard off; screen dark
  wifiOff();

  /* Both wing buttons wake the chip: C via ext0, A via ext1 (single-pin mask,
     so ALL_LOW just means "that pin low"). Internal RTC pull-ups must stay
     powered in deep sleep for ext1, hence the RTC_PERIPH domain option. */
  rtc_gpio_pullup_en((gpio_num_t)WAKE_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)WAKE_BTN);
  rtc_gpio_pullup_en((gpio_num_t)CFG_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)CFG_BTN);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BTN, 0);                   // press = LOW
  esp_sleep_enable_ext1_wakeup(1ULL << CFG_BTN, ESP_EXT1_WAKEUP_ALL_LOW);  // press = LOW

  if (wifiConfigured()) {                          // unconfigured devices only wake on a button
    uint64_t minutes = cache.lowBattery ? POLL_LOW_MIN : POLL_MIN;   // stretch the interval when low
    esp_sleep_enable_timer_wakeup(minutes * 60ULL * 1000000ULL);
  }
  esp_deep_sleep_start();
}

void setup() {
  pinMode(QT_POWER, OUTPUT);
  digitalWrite(QT_POWER, LOW);
  pinMode(WAKE_BTN, INPUT_PULLUP);
  pinMode(CFG_BTN, INPUT_PULLUP);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool configured = wifiConfigured();

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    setupLoop();                                   // button A: WiFi setup hotspot
  } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    if (!configured)        notConfiguredNotice(false);
    else if (waitForHold()) pollSequence(false);   // button C held: check messages now
    else                    composeLoop();         // button C tapped: typing session
  } else {                                         // timer wake or first power-on
    if (configured) pollSequence(cause == ESP_SLEEP_WAKEUP_UNDEFINED);
    else            notConfiguredNotice(true);     // cold boot: draw the hint screen
  }

  goToSleep();                                     // one job done -> sleep
}

void loop() { /* never reached */ }
