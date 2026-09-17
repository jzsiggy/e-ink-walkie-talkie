#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "Adafruit_ThinkInk.h"
#include "config.h"
#include "state.h"
#include "eink.h"

static ThinkInk_290_Grayscale4_EAAMFGN epd(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, &SPI);

static const char* OFFLINE_LABEL     = "disconnected...";   // top-right, built-in font
static const char* NO_SIGNAL_TITLE   = "No Signal";         // FreeSans 24pt, centred
static const char* NO_SIGNAL_SUB     = "mas eu te amo";     // built-in font, centred
static const int   NO_SIGNAL_TITLE_Y = 68;                  // baseline
static const int   NO_SIGNAL_SUB_Y   = 82;                  // top of glyphs

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

/* Normal layout: my last message on top, theirs below. When offline, the
   cached messages stay and a small marker flags them as possibly stale. */
static void drawMessages() {
  /* --- top pane, built-in font: what I last sent --- */
  epd.setFont(NULL);                               // built-in: y = top of text
  epd.setCursor(EINK_MARGIN, TOP_LABEL_Y);
  if (cache.mine.have) {
    epd.print("YOU  ");
    epd.print(cache.mine.time);
  } else {
    epd.print("YOU  (nothing sent yet)");          // edge case
  }
  if (cache.offline) printRightAligned(epd, OFFLINE_LABEL, TOP_LABEL_Y);
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

/* Cold boot with no WiFi and nothing cached: there is no stale content to
   show, so say so. */
static void drawNoSignal() {
  epd.setFont(&FreeSans24pt7b);
  printCentered(epd, NO_SIGNAL_TITLE, NO_SIGNAL_TITLE_Y);
  epd.setFont(NULL);
  printCentered(epd, NO_SIGNAL_SUB, NO_SIGNAL_SUB_Y);
}

void drawEink() {
  epd.begin(THINKINK_GRAYSCALE4);
  epd.clearBuffer();
  epd.setTextColor(EPD_BLACK);                     // all text in full black
  epd.setTextSize(1);
  epd.setTextWrap(false);                          // wrapPrint owns line breaks

  if (cache.offline && !cache.mine.have && !cache.theirs.have) drawNoSignal();
  else                                                          drawMessages();

  epd.setFont(NULL);                               // restore default
  epd.display();
  epd.powerDown();
}
