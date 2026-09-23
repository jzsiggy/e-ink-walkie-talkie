/* Compile-time configuration: device identity, pins, sizes, timing, layout.
 * Credentials live in secrets.h (included at the bottom). */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* ---- per-device settings ---- */
constexpr long     MY_USER_ID = 1;     // your row's id in the users table
constexpr int      TZ_OFFSET  = -3;    // hours from UTC: Sao Paulo (no DST)
constexpr uint64_t POLL_MIN   = 1;     // minutes between mailbox checks

/* ---- pins: values proven during bring-up ---- */
constexpr int8_t  EPD_CS    = 15;
constexpr int8_t  EPD_DC    = 33;
constexpr int8_t  SRAM_CS   = 32;
constexpr int8_t  EPD_RESET = -1;
constexpr int8_t  EPD_BUSY  = -1;
constexpr int     QT_POWER  = 2;       // HIGH = OLED + keyboard powered
constexpr int     WAKE_BTN  = 13;      // wing button on the D13 position (verify by pressing)
constexpr uint8_t OLED_ADDR = 0x3D;
constexpr uint8_t KBD_ADDR  = 0x5F;
constexpr int     BATT_PIN  = 35;      // on-board 2x200k divider (input-only GPIO, A13)

/* ---- buffers ---- */
constexpr size_t MSG_BUF     = 220;    // stored message text incl. NUL
constexpr size_t NAME_BUF    = 24;
constexpr size_t TIME_BUF    = 16;     // "DD Mon HH:MM"
constexpr size_t COMPOSE_MAX = 190;    // max characters typed per message

/* ---- timing ---- */
constexpr unsigned long WIFI_TIMEOUT_MS = 10000;
constexpr unsigned long COMPOSE_IDLE_MS = 60000;   // no keystroke -> give up (draft lost)
constexpr unsigned long QT_SETTLE_MS    = 500;     // OLED power-on settle

/* ---- battery ---- */
constexpr int BATT_SAMPLES    = 16;    // ADC reads averaged per measurement
constexpr int BATT_HYSTERESIS = 7;     // points past a 10% boundary before the step changes
constexpr int      BATT_LOW_PCT = 20;  // displayed step at or below this is "low"
constexpr uint64_t POLL_LOW_MIN = 20;  // minutes between polls when low

/* ---- e-ink layout (2.9" 296x128, landscape) ---- */
constexpr int EINK_W        = 296;
constexpr int EINK_H        = 128;
constexpr int EINK_MARGIN   = 4;
constexpr int EINK_TEXT_W   = EINK_W - 2 * EINK_MARGIN;   // 288 px usable
constexpr int TOP_LABEL_Y   = 4;       // built-in font: y = top of glyph
constexpr int TOP_MSG_Y     = 15;
constexpr int TOP_LINE_H    = 10;
constexpr int TOP_MAX_LINES = 2;
constexpr int DIVIDER_Y     = 40;
constexpr int BOT_LABEL_Y   = 46;
constexpr int BOT_MSG_Y     = 72;      // FreeSans 9pt: y = baseline
constexpr int BOT_LINE_H    = 16;
constexpr int BOT_MAX_LINES = 4;

#include "secrets.h"
