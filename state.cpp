#include <Arduino.h>
#include "state.h"

/* Zero-initialised on cold boot; preserved across deep sleep. */
RTC_DATA_ATTR Cache cache = {};
