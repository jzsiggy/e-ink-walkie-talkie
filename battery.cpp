#include <Arduino.h>
#include "config.h"
#include "battery.h"

/* Resting single-cell LiPo curve, mV -> %. 100% is a rested full cell (a cell
   only sits at 4.20 V while charging); 0% is where WiFi bursts start browning
   the board out, so the displayed range is the usable one. */
struct Point { int mv; int pct; };
static const Point CURVE[] = {
  {4150, 100}, {4100, 92}, {4000, 80}, {3900, 65},
  {3800, 48},  {3700, 28}, {3600, 12}, {3500, 0},
};
static const int N = sizeof CURVE / sizeof CURVE[0];

static int batteryMillivolts() {
  analogSetPinAttenuation(BATT_PIN, ADC_11db);       // full 0..~3.1 V range on the pin
  uint32_t sum = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) sum += analogReadMilliVolts(BATT_PIN);
  return (int)(sum / BATT_SAMPLES) * 2;              // undo the divider
}

int batteryPercent() {
  int mv = batteryMillivolts();
  if (mv >= CURVE[0].mv)     return 100;
  if (mv <= CURVE[N - 1].mv) return 0;
  for (int i = 1; i < N; i++) {
    if (mv >= CURVE[i].mv) {                         // between CURVE[i-1] (hi) and CURVE[i] (lo)
      const Point& hi = CURVE[i - 1];
      const Point& lo = CURVE[i];
      return lo.pct + (mv - lo.mv) * (hi.pct - lo.pct) / (hi.mv - lo.mv);
    }
  }
  return 0;
}

int batteryStep(int pct) {
  return ((pct + 5) / 10) * 10;
}
