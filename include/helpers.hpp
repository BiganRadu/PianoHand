#pragma once
#include <Arduino.h>
#include <string>
#include <ESP32Servo.h>
#include "config.hpp"

// Forward declaration of the global servo array
extern Servo degete[5];

/**
 * @brief Represents the three operating modes of the PianoHand system.
 */
enum class Mode : uint8_t {
    MODE_1 = 0, ///< Play hardcoded melody (array)
    MODE_2 = 1, ///< Record audio from mic, detect and play notes
    MODE_3 = 2  ///< Read WAV file from SD card, run FFT and play notes
};

/**
 * @brief Struct representing a musical note, its duration, finger index, and name.
 */
struct Note {
    int durationMs;    ///< Duration of the note in milliseconds
    int index;         ///< The mapped physical finger/servo index (0 to 4)
    std::string name;  ///< Friendly name of the note (e.g. "DO", "RE")
};

/**
 * @brief Actuates a physical servo finger to press a piano key based on the Note structure parameters.
 * 
 * @param note The Note structure containing the finger/servo index and duration.
 */
inline void playNote(const Note& note) {
  if (note.index >= 0 && note.index < 5) {
    degete[note.index].write(65);
  }
  
  // Press duration minus latch delay, if long enough
  delay(note.durationMs > 200 ? note.durationMs - 200 : 0);

  if (note.index >= 0 && note.index < 5) {
    degete[note.index].write(94);
  }
  
  // Post-release recovery buffer
  delay(200);
}
