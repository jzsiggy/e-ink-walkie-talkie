/* WiFi setup hotspot: open AP + captive portal page to pick a network. */
#pragma once

enum PortalResult {
  PORTAL_SAVED,       // new credentials tested and stored
  PORTAL_FORGOTTEN,   // stored credentials cleared
  PORTAL_TIMEOUT,     // nobody used it for PORTAL_TIMEOUT_MS
  PORTAL_CANCELLED    // Esc on the keyboard
};

PortalResult portalRun();   // blocks; narrates on the OLED; tears the AP down before returning
