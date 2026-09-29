#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "Adafruit_ThinkInk.h"
#include "config.h"
#include "state.h"
#include "battery.h"
#include "net.h"
#include "eink.h"

static ThinkInk_290_Grayscale4_EAAMFGN epd(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, &SPI);

static const char* OFFLINE_LABEL     = "disconnected...";   // top-right, left of the battery
static const char* NO_SIGNAL_TITLE   = "No Signal";         // configured but unreachable, nothing cached
static const char* NO_SIGNAL_SUB     = "mas eu te amo";
static const char* WELCOME_TITLE     = "Hi, love!";         // no WiFi stored yet
static const char* WELCOME_HINT      = "press the top left button to configure wifi";
static const int   BIG_TITLE_Y       = 68;                  // FreeSans 24pt baseline (both full screens)
static const int   BIG_SUB_Y         = 82;                  // built-in font top (both full screens)

/* Text measurement in the currently selected font. Requires text wrap to be
   off on `g`, otherwise GFX folds long strings at the panel edge and the width
   comes back wrong. */
static void measure(Adafruit_GFX& g, const String& s, int16_t& x1, uint16_t& w) {
  int16_t y1;
  uint16_t h;
  g.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
}

static int textW(Adafruit_GFX& g, const String& s) {
  int16_t x1; uint16_t w;
  measure(g, s, x1, w);
  return w;
}

static void printCentered(Adafruit_GFX& g, const char* s, int y) {
  int16_t x1; uint16_t w;
  measure(g, s, x1, w);
  g.setCursor((EINK_W - (int)w) / 2 - x1, y);
  g.print(s);
}

static void printRightAligned(Adafruit_GFX& g, const char* s, int y) {
  int16_t x1; uint16_t w;
  measure(g, s, x1, w);
  g.setCursor(EINK_W - EINK_MARGIN - (int)w - x1, y);
  g.print(s);
}

/* Word wrap by measured pixel width. A word wider than a line is split by
   characters. If the text does not fit in maxLines, the last line ends in "..". */
static void wrapPrint(Adafruit_GFX& g, const char* s, int x, int y,
                      int maxWidthPx, int maxLines, int lineH) {
  String rest = s;
  rest.trim();
  int cy = y;
  for (int line = 0; line < maxLines && rest.length() > 0; line++) {
    int take = 0;                                  // chars of `rest` that go on this line
    while (true) {                                 // grow word by word while it fits
      int next = rest.indexOf(' ', take + 1);
      if (next < 0) next = rest.length();
      if (textW(g, rest.substring(0, next)) > maxWidthPx) break;
      take = next;
      if (take >= (int)rest.length()) break;
    }
    if (take == 0) {                               // first word alone is too wide
      take = 1;
      while (take < (int)rest.length() &&
             textW(g, rest.substring(0, take + 1)) <= maxWidthPx) take++;
    }
    String out = rest.substring(0, take);
    rest = rest.substring(take);
    rest.trim();

    bool lastLine = (line == maxLines - 1);
    if (lastLine && rest.length() > 0) {           // truncated: mark it
      while (out.length() > 0 && textW(g, out + "..") > maxWidthPx) out.remove(out.length() - 1);
      out.trim();
      out += "..";
    }
    g.setCursor(x, cy);
    g.print(out);
    cy += lineH;
  }
}

/* Top-right corner: battery step, preceded by the stale marker when offline.
   Built-in font. Longest case "disconnected...  100%" is 126 px, clear of the
   top label which ends by x~142. */
static void drawCorner(int batteryPct, bool showOffline) {
  char corner[32];
  if (showOffline) snprintf(corner, sizeof corner, "%s  %d%%", OFFLINE_LABEL, batteryPct);
  else               snprintf(corner, sizeof corner, "%d%%", batteryPct);
  epd.setFont(NULL);
  printRightAligned(epd, corner, TOP_LABEL_Y);
}

/* Normal layout: my last message on top, theirs below. When offline, the
   cached messages stay and a small marker flags them as possibly stale. */
static void drawMessages(int batteryPct) {
  /* --- top pane, built-in font: what I last sent --- */
  epd.setFont(NULL);                               // built-in: y = top of text
  epd.setCursor(EINK_MARGIN, TOP_LABEL_Y);
  if (cache.mine.have) {
    epd.print("YOU  ");
    epd.print(cache.mine.time);
  } else {
    epd.print("YOU  (nothing sent yet)");          // edge case
  }
  drawCorner(batteryPct, cache.offline);
  if (cache.mine.have) {
    wrapPrint(epd, cache.mine.text, EINK_MARGIN, TOP_MSG_Y, EINK_TEXT_W, TOP_MAX_LINES, TOP_LINE_H);
  }

  epd.drawLine(0, DIVIDER_Y, EINK_W - 1, DIVIDER_Y, EPD_LIGHT);   // the ONLY light-gray element

  /* --- bottom pane: label built-in, message in FreeSans 9pt --- */
  if (cache.theirs.have) {
    epd.setFont(NULL);
    epd.setCursor(EINK_MARGIN, BOT_LABEL_Y);
    epd.print(cache.theirs.name);
    epd.print("  ");
    epd.print(cache.theirs.time);
    epd.setFont(&FreeSans9pt7b);                   // GFX font: y = BASELINE
    wrapPrint(epd, cache.theirs.text, EINK_MARGIN, BOT_MSG_Y, EINK_TEXT_W, BOT_MAX_LINES, BOT_LINE_H);
  } else {
    epd.setFont(&FreeSans9pt7b);                   // edge case: empty thread
    wrapPrint(epd, "No messages yet. Say hi!", EINK_MARGIN, BOT_MSG_Y, EINK_TEXT_W, BOT_MAX_LINES, BOT_LINE_H);
  }
}

/* One big centred line in FreeSans 24pt and one small one beneath it. */
static void drawFullScreen(int batteryPct, const char* title, const char* sub) {
  drawCorner(batteryPct, false);                   // marker not needed: the screen itself says it
  epd.setFont(&FreeSans24pt7b);
  printCentered(epd, title, BIG_TITLE_Y);
  epd.setFont(NULL);
  printCentered(epd, sub, BIG_SUB_Y);
}

/* Configured, but a cold boot could not reach the network and nothing is cached. */
static void drawNoSignal(int batteryPct) { drawFullScreen(batteryPct, NO_SIGNAL_TITLE, NO_SIGNAL_SUB); }

/* No WiFi stored yet: greet and point at the setup button. */
static void drawWelcome(int batteryPct)  { drawFullScreen(batteryPct, WELCOME_TITLE, WELCOME_HINT); }

void drawEink() {
  int batteryPct = batteryStep(batteryPercent());  // callers have WiFi off here: idle enough to read
  cache.batteryPct = batteryPct;                   // the cache holds exactly what the panel shows

  epd.begin(THINKINK_GRAYSCALE4);
  epd.clearBuffer();
  epd.setTextColor(EPD_BLACK);                     // all text in full black
  epd.setTextSize(1);
  epd.setTextWrap(false);                          // wrapPrint owns line breaks

  bool unconfigured = !wifiConfigured();
  bool nothingCached = !cache.mine.have && !cache.theirs.have;
  if      (unconfigured)                     drawWelcome(batteryPct);
  else if (cache.offline && nothingCached)   drawNoSignal(batteryPct);
  else                                       drawMessages(batteryPct);

  epd.setFont(NULL);                               // restore default
  epd.display();
  epd.powerDown();
}
