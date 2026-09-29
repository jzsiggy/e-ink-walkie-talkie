#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "net.h"
#include "supabase.h"
#include "eink.h"
#include "ui.h"
#include "battery.h"
#include "portal.h"
#include "modes.h"

/* Records connectivity; true if the state flipped, meaning the e-ink needs a
   redraw so the "disconnected..." marker appears or clears. */
static bool markOffline(bool offline) {
  bool flipped = cache.offline != offline;
  cache.offline = offline;
  return flipped;
}

/* Redraw the e-ink after going offline, narrating the ~2 s pause on the OLED. */
static void showStale(const char* reason) {
  bool haveContent = cache.mine.have || cache.theirs.have;
  oledMsg(reason, haveContent ? "Marking stale..." : "No signal screen...");
  drawEink();
}

/* True when the rounded battery level differs from what the e-ink shows and the
   raw value is clearly past the boundary, so noise around e.g. 65% cannot flip
   the panel between 60 and 70 on consecutive polls. */
static bool batteryStepChanged(int pct) {
  return batteryStep(pct) != cache.batteryPct && abs(pct - cache.batteryPct) > BATT_HYSTERESIS;
}

/* Three on/off flashes, ~2 s total, so a low battery is hard to miss. */
static void flashChargeMe() {
  for (int i = 0; i < 3; i++) {
    oledMsg("Charge me!");
    delay(400);
    oledClear();
    delay(300);
  }
}

/* Button C woke us. True if it is still held after HOLD_POLL_MS, meaning the
   user wants a poll rather than a compose session. The hint on the OLED makes
   the hold discoverable; a quick tap usually releases during the OLED settle
   and never sees it. */
bool waitForHold() {
  peripheralsOn();                                 // includes the QT_SETTLE_MS wait
  if (digitalRead(WAKE_BTN) == HIGH) return false; // already released: a tap
  oledMsg("Hold to check", "messages...");
  unsigned long t0 = millis();
  while (digitalRead(WAKE_BTN) == LOW) {
    if (millis() - t0 >= HOLD_POLL_MS) return true;   // hint stays up until the poll draws its first screen
    delay(10);
  }
  return false;
}

/* Button-wake path: the typing session. No polling can start in here:
   the POLL_MIN timer is only armed at sleep time. */
void composeLoop() {
  if (!peripheralsOn()) return;

  String buf = "";
  unsigned long lastKey = millis();
  oledMsg("Type", "Press Enter to send", "Press Esc to cancel");

  while (millis() - lastKey < COMPOSE_IDLE_MS) {   // idle -> give up (draft lost)
    char k = readKey();
    if (k == 0) { delay(20); continue; }
    lastKey = millis();

    if (k == 0x1B) return;                         // Esc = cancel

    if (k == 0x0D) {                               // Enter -> send
      if (buf.length() == 0) return;
      oledMsg("Sending...");
      if (!wifiConnect()) {
        oledMsg("No WiFi.", "Try again later");
        delay(2500);
        if (markOffline(true)) showStale("No WiFi.");
        return;
      }
      if (postMessage(buf.c_str())) {
        FetchResult r = fetchLatest();             // my row back + anything new from them
        wifiOff();
        markOffline(r == FETCH_FAILED);
        oledMsg("Sent!", r == FETCH_FAILED ? "Refresh failed" : "");
        drawEink();
        delay(800);
      } else {
        wifiOff();
        oledMsg("Send failed.", "Server error");
        delay(2500);
        if (markOffline(true)) showStale("Send failed.");
      }
      return;
    }

    if (k == 0x08) {                               // backspace
      if (buf.length()) buf.remove(buf.length() - 1);
    } else if (k >= 0x20 && k < 0x7F && buf.length() < COMPOSE_MAX) {
      buf += k;
    }
    oledShowDraft(buf);
  }
}

/* Timer-wake path. Narrated on the OLED so a watching human knows the device
   is busy. The keyboard is never read in here, so user input is inherently
   blocked until the screen goes dark and the device sleeps. */
void pollSequence(bool firstBoot) {
  peripheralsOn();                                 // status only; carry on even if the OLED is missing
  int pct = batteryPercent();                      // before WiFi: board idle, no load sag
  cache.lowBattery = batteryStep(pct) <= BATT_LOW_PCT;
  char batt[16];
  snprintf(batt, sizeof batt, "Battery %d%%", batteryStep(pct));
  oledMsg("Waking up...", batt);
  delay(1000);                                     // long enough to read the battery figure

  oledMsg("Connecting to WiFi...");
  if (!wifiConnect()) {
    oledMsg("No WiFi.", "Trying again later");
    delay(1200);
    if (markOffline(true)) showStale("No WiFi.");
  } else {
    oledMsg("Checking messages...");
    FetchResult r = fetchLatest();
    wifiOff();

    if (r == FETCH_FAILED) {
      oledMsg("Server error.", "Trying again later");
      delay(1200);
      if (markOffline(true)) showStale("Server error.");
    } else {
      bool backOnline = markOffline(false);
      if (r == FETCH_CHANGED) {
        oledMsg("New message!", "Updating display...");
        drawEink();                                // ~2 s flash
        oledMsg("Done!");
      } else if (firstBoot || backOnline || batteryStepChanged(pct)) {   // unchanged state, marker cleared, or battery stepped
        oledMsg("Updating display...");
        drawEink();
        oledMsg("Done!");
      } else {
        oledMsg("Nothing new.");
      }
      delay(700);
    }
  }

  if (cache.lowBattery) flashChargeMe();         // after the poll, before "Falling asleep..."
}

/* Button A wake: run the hotspot until credentials are saved, forgotten, or
   nobody uses it for PORTAL_TIMEOUT_MS. */
void setupLoop() {
  peripheralsOn();
  PortalResult r = portalRun();
  if (r == PORTAL_SAVED) {
    pollSequence(true);                            // draw live data right away
  } else if (r == PORTAL_FORGOTTEN) {
    oledMsg("WiFi forgotten", "Updating display...");
    drawEink();                                    // unconfigured -> No Signal + hint
  } else if (r == PORTAL_CANCELLED) {
    oledMsg("Setup cancelled");
    delay(1000);
  } else {
    oledMsg("Setup timed out");
    delay(1000);
  }
}

/* Any wake while no WiFi is stored. The e-ink is drawn only on cold boot
   (drawPanel) so a stray button press does not flash the panel. */
void notConfiguredNotice(bool drawPanel) {
  peripheralsOn();
  oledMsg("No Signal");                            // the e-ink carries the instructions
  if (drawPanel) drawEink();                       // ~2 s, message stays visible meanwhile
  else           delay(2500);
}
