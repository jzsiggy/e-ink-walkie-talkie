#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "ui.h"

static const int OLED_W      = 128;
static const int OLED_H      = 64;
static const int OLED_LINE_H = 10;                 // 8 px glyphs + 2 px gap

static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static bool ready = false;                       // rail up and OLED initialised this wake

/* Idempotent: a second call in the same wake returns at once, so paths that
   init twice (hold check then compose, setup then poll) pay no extra settle. */
bool peripheralsOn() {
  if (ready) return true;
  digitalWrite(QT_POWER, HIGH);
  delay(QT_SETTLE_MS);                             // OLED settle time
  Wire.begin();
  bool ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (ok) {
    oled.clearDisplay();                           // the panel RAM powers up as noise and begin()
    oled.display();                                // turns the display on: blank it right away
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);                       // measured layout; the draft turns wrap on itself
    ready = true;
  }
  return ok;
}

void peripheralsOff() {
  digitalWrite(QT_POWER, LOW);
  ready = false;
}

/* Print `s` horizontally centred with its top at `y`. Text wrap must be off,
   otherwise getTextBounds folds long strings at the panel edge. A line wider
   than the panel is left-aligned rather than pushed off-screen. */
static void oledPrintCentered(const char* s, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  int x = (OLED_W - (int)w) / 2 - x1;
  oled.setCursor(x < 0 ? 0 : x, y);
  oled.print(s);
}

/* System/status message: up to three lines, centred on both axes. Trailing
   empty lines are dropped; an empty line in the middle is kept as a blank row. */
void oledMsg(const char* line1, const char* line2, const char* line3) {
  const char* lines[3] = { line1, line2, line3 };
  int count = 3;
  while (count > 1 && lines[count - 1][0] == 0) count--;
  int y = (OLED_H - count * OLED_LINE_H) / 2;
  oled.clearDisplay();
  for (int i = 0; i < count; i++) oledPrintCentered(lines[i], y + i * OLED_LINE_H);
  oled.display();
}

void oledClear() {
  oled.clearDisplay();
  oled.display();
}

/* Text being typed: top-left, wrapping character-wise like a terminal. */
void oledShowDraft(const String& draft) {
  oled.clearDisplay();
  oled.setTextWrap(true);
  oled.setCursor(0, 0);
  oled.println(draft);
  oled.setTextWrap(false);
  oled.display();
}

char readKey() {
  Wire.requestFrom((uint8_t)KBD_ADDR, (uint8_t)1);   // CardKB: reads only
  return Wire.available() ? Wire.read() : 0;
}
