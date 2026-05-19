#pragma once
#include <Arduino.h>
#include "mode_interface.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "lcd_utils.hpp"

/**
 * @brief Array of hardcoded notes representing the melody played in Mode 1.
 * 
 * Each note has a default duration and is associated with a finger index 
 * from 0 to 4 (DO to SOL).
 */
inline const Note kMode1Notes[kNoteCount] = {
  {kNoteDurationMs, 0, "DO"}, {kNoteDurationMs, 1, "RE"}, {kNoteDurationMs, 2, "MI"}, {kNoteDurationMs, 3, "FA"}, {kNoteDurationMs, 4, "SOL"},
  {kNoteDurationMs, 0, "DO"}, {kNoteDurationMs, 1, "RE"}, {kNoteDurationMs, 2, "MI"}, {kNoteDurationMs, 3, "FA"}, {kNoteDurationMs, 4, "SOL"},
  {kNoteDurationMs, 0, "DO"}, {kNoteDurationMs, 1, "RE"}, {kNoteDurationMs, 2, "MI"}, {kNoteDurationMs, 3, "FA"}, {kNoteDurationMs, 4, "SOL"},
  {kNoteDurationMs, 0, "DO"}, {kNoteDurationMs, 1, "RE"}, {kNoteDurationMs, 2, "MI"}, {kNoteDurationMs, 3, "FA"}, {kNoteDurationMs, 4, "SOL"}
};

/**
 * @class Mode1
 * @brief Represents the first mode of operation: playing a hardcoded melody.
 * 
 * In this mode, the system plays the pre-defined array of notes (kMode1Notes) 
 * using the servo-controlled fingers and updates the LCD display accordingly.
 */
class Mode1 : public IMode {
  public:
    /**
     * @brief Prints the Mode 1 menu details on the LCD screen.
     */
    void print() override {
      PrintLinePadded(0, "Mode 1");
      PrintLinePadded(1, "Press to play");
    }

    /**
     * @brief Plays the hardcoded melody by actuating the servo motors in sequence.
     * 
     * Iterates through the kMode1Notes array, maps the note name to the corresponding 
     * servo index, writes to the servo to press the key, delays, writes to release 
     * the key, and updates the LCD screen with the current note progress.
     */
    void play() override {
      char prevLine0[17];
      char prevLine1[17];
      memcpy(prevLine0, gLcdLine0, sizeof(prevLine0));
      memcpy(prevLine1, gLcdLine1, sizeof(prevLine1));

      PrintLinePadded(0, "Playing...");
      PrintLinePadded(1, "");

      for (int i = 0; i < kNoteCount; ++i) {
        int chunkStart = (i / kNoteChunkSize) * kNoteChunkSize;
        char lineBuf[17];
        BuildNoteLine(lineBuf, sizeof(lineBuf), kMode1Notes, kNoteCount, chunkStart, i);

        PrintLinePadded(1, lineBuf);
        playNote(kMode1Notes[i]);
      }

      PrintLinePadded(0, prevLine0);
      PrintLinePadded(1, prevLine1);
    }
};

