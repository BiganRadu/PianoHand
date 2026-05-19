/**
 * ESP32 + 1602 LCD (I2C) + MicroSD (SPI) + MAX4466 microphone + 5x Servo-controlled fingers
 *
 * LCD wiring:
 *  SDA -> GPIO21
 *  SCL -> GPIO22
 *  VCC -> 5V
 *  GND -> Common Ground
 *
 * MicroSD wiring:
 *  CS   -> GPIO5
 *  SCK  -> GPIO18
 *  MISO -> GPIO19
 *  MOSI -> GPIO23
 *  VCC  -> 5V (VIN)
 *  GND  -> Common Ground
 *
 * MAX4466 mic wiring:
 *  OUT -> GPIO34
 *  VCC -> 3.3V
 *  GND -> Common Ground
 * 
 * Servo wiring:
 *  Servo 1 (DO)  -> GPIO12
 *  Servo 2 (RE)  -> GPIO13
 *  Servo 3 (MI)  -> GPIO14
 *  Servo 4 (FA)  -> GPIO27
 *  Servo 5 (SOL) -> GPIO26
 *  VCC          -> External 5V power supply (VIN)
 *  GND          -> Common Ground
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <ESP32Servo.h>

#include "helpers.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "lcd_utils.hpp"
#include "audio_dsp.hpp"
#include "sd_utils.hpp"


// DSP real and imaginary frequency domain arrays (2048 bins)
float gHpsReal[2048];
float gHpsImag[2048];
// DSP raw time domain audio buffer
int16_t gHpsRaw[2048];

// Array of 5 Servo objects controlling physical finger actuators
Servo degete[5];
// Liquid Crystal Display driver instance (I2C)
LiquidCrystal_I2C lcd(kLcdAddress, kLcdCols, kLcdRows);

// Global status tracking variables
bool gSdOk = false;
char gLcdLine0[17] = "                ";
char gLcdLine1[17] = "                ";
Mode current_mode;

// Debouncing variables for Button 1 (Mode switch)
int gBtn1StableState = HIGH;
int gBtn1LastReading = HIGH;
unsigned long gBtn1LastChangeMs = 0;

// Debouncing variables for Button 2 (Execute Mode)
int gBtn2StableState = HIGH;
int gBtn2LastReading = HIGH;
unsigned long gBtn2LastChangeMs = 0;


// --- Include Mode Implementations ---
#include "mode_interface.hpp"
#include "mode1.hpp"
#include "mode2.hpp"
#include "mode3.hpp"

// Polymorphic pointers to system operating modes
IMode* modes[3] = { new Mode1(), new Mode2(), new Mode3() };

/**
 * @brief Main system initialization routine.
 * 
 * Performs hardware configurations:
 * - Initializes Serial interface at 115200 baud.
 * - Starts I2C communication (Wire).
 * - Configures ADC resolution (12-bit) and attenuation for the microphone.
 * - Builds the chromatic scale frequency lookup table.
 * - Attaches and centers the 5 physical finger servos.
 * - Configures button inputs with internal pullups.
 * - Mounts and tests the MicroSD card.
 * - Initializes the LCD screen and prints startup greeting message.
 * - Sets the initial mode to Mode 1 (Hardcoded Melody).
 */
void setup() {
  Serial.begin(115200);

  Wire.begin(kI2cSda, kI2cScl);

  analogReadResolution(12);
  analogSetPinAttenuation(kMicPin, ADC_11db);
  buildChromaticScale();

  for (int i = 0; i < 5; i++) {
    degete[i].attach(kServoPins[i]);
    degete[i].write(94);
  }

  pinMode(kBtn1Pin, INPUT_PULLUP);
  pinMode(kBtn2Pin, INPUT_PULLUP);

  gSdOk = InitAndTestSd();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  PrintLinePadded(0, "ESP32 + LCD");
  PrintLinePadded(1, "Starting...");
  delay(1000);
  lcd.clear();

  current_mode = Mode::MODE_1;
  gBtn1StableState = digitalRead(kBtn1Pin);
  gBtn1LastReading = gBtn1StableState;
  gBtn2StableState = digitalRead(kBtn2Pin);
  gBtn2LastReading = gBtn2StableState;

  lcd.clear();
  modes[static_cast<int>(current_mode)]->print();
}

/**
 * @brief Main execution loop of the program.
 * 
 * Monitored inside the loop:
 * - Debounces BTN1 input: on press, advances the current system operating mode,
 *   clears the display, and updates it with the new mode's printout.
 * - Debounces BTN2 input: on press, launches the active mode's play() routine
 *   to trigger playback, note processing, or microphone recording.
 */
void loop() {
  unsigned long now = millis();
  int reading = digitalRead(kBtn1Pin);
  if (reading != gBtn1LastReading) {
    gBtn1LastChangeMs = now;
    gBtn1LastReading = reading;
  }

  if ((now - gBtn1LastChangeMs) > kDebounceMs && reading != gBtn1StableState) {
    gBtn1StableState = reading;
    if (gBtn1StableState == LOW) {
      int nextMode = static_cast<int>(current_mode) + 1;
      current_mode = static_cast<Mode>(nextMode % 3);
      lcd.clear();
      modes[static_cast<int>(current_mode)]->print();
    }
  }

  int reading2 = digitalRead(kBtn2Pin);
  if (reading2 != gBtn2LastReading) {
    gBtn2LastChangeMs = now;
    gBtn2LastReading = reading2;
  }

  if ((now - gBtn2LastChangeMs) > kDebounceMs && reading2 != gBtn2StableState) {
    gBtn2StableState = reading2;
    if (gBtn2StableState == LOW) {
      modes[static_cast<int>(current_mode)]->play();
    }
  }
}
