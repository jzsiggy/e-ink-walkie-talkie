#include <string.h>
#include <stdio.h>
#include "textfmt.h"

/* ------------------------------ ASCII shaping ------------------------------ */

/* U+00C0..U+00FF (UTF-8 C3 80..C3 BF) -> nearest ASCII letter. */
static const char LATIN1[65] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";

static bool isCont(unsigned char b) { return (b & 0xC0) == 0x80; }

void toDisplayAscii(const char* in, char* out, size_t n) {
  if (n == 0) return;
  size_t len = 0;
  bool pendingSpace = false;                     // collapse runs; never emit a leading space

  auto emit = [&](char c) {
    if (c == ' ') { if (len > 0) pendingSpace = true; return; }
    if (pendingSpace) { if (len + 1 < n) out[len++] = ' '; pendingSpace = false; }
    if (len + 1 < n) out[len++] = c;
  };

  const unsigned char* p = (const unsigned char*)in;
  while (*p) {
    unsigned char b = *p;
    if (b < 0x80) {                              // plain ASCII
      if (b == ' ' || b == '\n' || b == '\r' || b == '\t') emit(' ');
      else if (b >= 0x20 && b < 0x7F)            emit((char)b);
      p++;
      continue;
    }
    int seq = (b >= 0xF0) ? 4 : (b >= 0xE0) ? 3 : (b >= 0xC0) ? 2 : 0;
    bool ok = seq > 0;
    for (int i = 1; ok && i < seq; i++) ok = isCont(p[i]);   // stops at NUL too (not a continuation)
    if (!ok) { p++; continue; }                  // stray/truncated byte: drop it alone

    if (seq == 2 && b == 0xC3) {
      emit(LATIN1[p[1] - 0x80]);
    } else if (seq == 2 && b == 0xC2) {
      switch (p[1]) {
        case 0xA0: emit(' '); break;             // nbsp
        case 0xA1: emit('!'); break;             // inverted !
        case 0xBF: emit('?'); break;             // inverted ?
        case 0xAA: emit('a'); break;             // ª
        case 0xBA: emit('o'); break;             // º
        default: break;
      }
    } else if (seq == 3 && b == 0xE2 && p[1] == 0x80) {
      switch (p[2]) {
        case 0x98: case 0x99: emit('\''); break; // ‘ ’
        case 0x9C: case 0x9D: emit('"');  break; // “ ”
        case 0x93: case 0x94: emit('-');  break; // – —
        case 0xA6: emit('.'); emit('.'); emit('.'); break;   // …
        default: break;
      }
    }                                            // anything else (emoji, CJK...) is dropped
    p += seq;
  }
  out[len] = '\0';
}

/* ------------------------------ timestamps -------------------------------- */

static bool digits(const char* s, int count) {
  for (int i = 0; i < count; i++) if (s[i] < '0' || s[i] > '9') return false;
  return true;
}
static int num(const char* s, int count) {
  int v = 0;
  for (int i = 0; i < count; i++) v = v * 10 + (s[i] - '0');
  return v;
}

/* Proleptic Gregorian <-> days since 1970-01-01 (Howard Hinnant's algorithms). */
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
static void civilFromDays(long z, int& y, int& m, int& d) {
  z += 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp  = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)((long)yoe + era * 400 + (m <= 2));
}

void isoToLocalStamp(const char* iso, int tzOffsetHours, char* out, size_t n) {
  static const char* MON[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  bool ok = strlen(iso) >= 16 &&
            digits(iso, 4) && iso[4] == '-' && digits(iso + 5, 2) && iso[7] == '-' &&
            digits(iso + 8, 2) && (iso[10] == 'T' || iso[10] == ' ') &&
            digits(iso + 11, 2) && iso[13] == ':' && digits(iso + 14, 2);
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  if (ok) {
    y = num(iso, 4); mo = num(iso + 5, 2); d = num(iso + 8, 2);
    h = num(iso + 11, 2); mi = num(iso + 14, 2);
    ok = mo >= 1 && mo <= 12 && d >= 1 && d <= 31 && h <= 23 && mi <= 59;
  }
  if (!ok) { snprintf(out, n, "--:--"); return; }

  long minutes = daysFromCivil(y, mo, d) * 1440L + h * 60L + mi + (long)tzOffsetHours * 60L;
  long days = minutes / 1440, rem = minutes % 1440;
  if (rem < 0) { rem += 1440; days--; }
  civilFromDays(days, y, mo, d);
  snprintf(out, n, "%02d %s %02ld:%02ld", d, MON[mo - 1], rem / 60, rem % 60);
}
