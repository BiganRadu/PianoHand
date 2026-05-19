#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include "helpers.hpp"
#include "config.hpp"

/**
 * @file globals.hpp
 * @brief Declares shared instances, buffer regions, and volatile tracking states 
 *        used across multiple translation units.
 */

// Hardware instances
extern Servo degete[5];
extern LiquidCrystal_I2C lcd;

// Global flags and state
extern bool gSdOk;
extern char gLcdLine0[17];
extern char gLcdLine1[17];
extern Mode current_mode;

// Button debouncing states
constexpr uint32_t kDebounceMs = 50;
extern int gBtn1StableState;
extern int gBtn1LastReading;
extern unsigned long gBtn1LastChangeMs;
extern int gBtn2StableState;
extern int gBtn2LastReading;
extern unsigned long gBtn2LastChangeMs;


// DSP buffers shared between recording and processing phases
extern float gHpsReal[2048];
extern float gHpsImag[2048];
extern int16_t gHpsRaw[2048];
