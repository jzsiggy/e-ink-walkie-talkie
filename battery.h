/* LiPo level from the Feather V2's on-board 2x200k divider (no fuel gauge).
   About +/-10 points mid-curve; meant to be shown rounded. */
#pragma once

int batteryPercent();       // 0..100, unrounded, from an averaged calibrated ADC read
int batteryStep(int pct);   // nearest multiple of 10
