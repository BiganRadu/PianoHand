#pragma once
#include <Arduino.h>
#include <cstring>
#include "helpers.hpp"
#include "globals.hpp"

/**
 * @brief Prints a line on the LiquidCrystal I2C display, padding any unused
 *        columns with spaces to completely overwrite old content.
 * 
 * @param row The LCD row to write to (0 or 1).
 * @param text The text to print.
 */
inline void PrintLinePadded(uint8_t row, const char *text) {
  lcd.setCursor(0, row);
  lcd.print(text);
  size_t len = strlen(text);
  for (size_t i = len; i < 16; ++i) {
    lcd.print(' ');
  }
  char *dest = (row == 0) ? gLcdLine0 : gLcdLine1;
  size_t copyLen = len > 16 ? 16 : len;
  memcpy(dest, text, copyLen);
  for (size_t i = copyLen; i < 16; ++i) {
    dest[i] = ' ';
  }
  dest[16] = '\0';
}

/**
 * @brief Builds a formatted space-separated string representation of notes
 *        within a specific chunk window for display.
 * 
 * @param buffer Output destination string buffer.
 * @param bufferSize Size of the destination buffer in bytes.
 * @param notes Pointer to the array of Note elements.
 * @param noteCount Total number of notes in the array.
 * @param startIndex The start index of the chunk window.
 * @param endIndex The end index (inclusive) of the chunk window.
 */
inline void BuildNoteLine(char *buffer, size_t bufferSize, const Note *notes, int noteCount, int startIndex, int endIndex) {
  size_t pos = 0;
  if (bufferSize == 0) {
    return;
  }
  buffer[0] = '\0';
  for (int i = startIndex; i <= endIndex && i < noteCount; ++i) {
    const char *noteName = notes[i].name.c_str();
    int written = snprintf(buffer + pos, bufferSize - pos, "%s%s", (i > startIndex) ? " " : "", noteName);
    if (written < 0) {
      break;
    }
    if (static_cast<size_t>(written) >= bufferSize - pos) {
      buffer[bufferSize - 1] = '\0';
      break;
    }
    pos += static_cast<size_t>(written);
  }
}



