#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "net.h"
#include "supabase.h"
#include "eink.h"
#include "ui.h"
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
  oledMsg("Waking up...");
  delay(300);

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
        oledMsg("New message!", "Updating e-ink...");
        drawEink();                                // ~2 s flash
        oledMsg("Done!");
      } else if (firstBoot || backOnline) {        // draw the (unchanged) state, or clear the marker
        oledMsg("Updating e-ink...");
        drawEink();
        oledMsg("Done!");
      } else {
        oledMsg("Nothing new.");
      }
      delay(700);
    }
  }

  oledMsg("Falling asleep...");
  delay(400);
}
