/* Message cache that survives deep sleep (RTC slow memory). */
#pragma once
#include <stdint.h>
#include "config.h"

struct MsgSlot {
  int64_t id;               // newest message id seen for this slot
  bool    have;             // false until the first message arrives
  char    text[MSG_BUF];
  char    time[TIME_BUF];   // local "HH:MM"
  char    name[NAME_BUF];   // sender's first name
};

struct Cache {
  MsgSlot mine;             // my last sent message
  MsgSlot theirs;           // newest message from anyone else
  bool    offline;          // last refresh failed; e-ink is showing the stale marker
  int     batteryPct;       // rounded value currently drawn on the e-ink (0 = not yet drawn)
};

extern Cache cache;
