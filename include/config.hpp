#pragma once
#include <Arduino.h>

/**
 * @file config.hpp
 * @brief Unified configuration file containing pin configurations, timings,
 *        music constants, and FFT/HPS variables.
 */
// Servo pin mappings (5 fingers)
constexpr int kServoPins[5] = {13, 14, 27, 32, 33};

// LCD I2C Configuration
constexpr int kI2cSda = 21;
constexpr int kI2cScl = 22;
constexpr uint8_t kLcdAddress = 0x3F;
constexpr uint8_t kLcdCols = 16;
constexpr uint8_t kLcdRows = 2;

// MicroSD SPI Configuration
constexpr int kSdCs = 5;
constexpr int kSdSck = 18;
constexpr int kSdMiso = 19;
constexpr int kSdMosi = 23;

// Microphone and Buttons GPIO
constexpr int kMicPin = 34;
constexpr int kBtn1Pin = 25;
constexpr int kBtn2Pin = 26;

// General music and note constants
constexpr int kNoteCount = 20;
constexpr int kNoteDurationMs = 1000;
constexpr int kNoteChunkSize = 5;

// HPS FFT (Harmonic Product Spectrum) Configuration
constexpr int kHpsFftSize = 256;
constexpr uint32_t kHpsSampleRate = 2048;
constexpr uint32_t kHpsSamplePeriodUs = 1000000UL / kHpsSampleRate;
constexpr int kHpsHarmonics = 2;
constexpr float kHpsRmsThresh = 0.02f;
constexpr int kHpsMedianRadius = 2;
constexpr int kHpsMinSegFrames = 3;
constexpr int kHpsMinDurMs = 200;
constexpr int kHpsMaxDurMs = 750;
constexpr float kHpsFrameMs = (float)kHpsFftSize / kHpsSampleRate * 1000.0f;
constexpr uint32_t kMaxRecordSec = 20;

// Snapping keys for playback mapping
inline const char* const kPianoKeys[5] = {"DO", "RE", "MI", "FA", "SOL"};
