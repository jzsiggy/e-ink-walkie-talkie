/* OLED status display + CardKB keyboard, both on the switched STEMMA QT rail. */
#pragma once
#include <Arduino.h>

bool peripheralsOn();                                   // power rail up, I2C + OLED init; true if OLED answered
void peripheralsOff();                                  // power rail down: screen dark, keyboard off
void oledMsg(const char* line1, const char* line2 = "", const char* line3 = "");   // up to 3 centred lines
void oledShowDraft(const String& draft);
char readKey();                                         // 0 when no key is pending
