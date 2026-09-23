/* Text shaping for the display. Pure C++ (no Arduino.h) so it can be compiled
   and checked on a desktop with clang. */
#pragma once
#include <stddef.h>

/* UTF-8 -> printable ASCII for GFX fonts (glyphs exist only for 0x20..0x7E).
   Transliterates Latin-1 letters (ã->a, ç->c ...), normalises curly quotes,
   dashes and ellipsis, drops everything else (emoji, controls), collapses
   whitespace and trims. Output is NUL-terminated within n bytes. */
void toDisplayAscii(const char* in, char* out, size_t n);

/* "2026-09-16T14:32:07.123+00:00" (UTC) -> "16 Sep 14:32" shifted by
   tzOffsetHours with correct day rollover. "--:--" if unparseable. */
void isoToLocalStamp(const char* iso, int tzOffsetHours, char* out, size_t n);
