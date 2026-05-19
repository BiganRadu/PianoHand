# 🎹 PianoHand

**PianoHand** is a modular, high-performance ESP32-based robotic hand system designed to play standard piano keys. The system uses a structured hardware interface comprising a high-resolution electret microphone, an SPI MicroSD card module, an I2C LCD display, dual tactile pushbuttons for control, and five SG90 servo-controlled fingers mapped to the five primary notes of the pentatonic scale (**DO, RE, MI, FA, SOL**).

The firmware utilizes digital signal processing (DSP) to analyze sound in both real-time (from the microphone) and file-based playback (from the SD card) using custom **Cooley-Tukey Fast Fourier Transform (FFT)** and **Harmonic Product Spectrum (HPS)** pitch-tracking algorithms to actuate the robotic fingers.

---

## 🛠️ Physical Components & Hardware Wiring

The system runs on an **ESP32 DevKit** board connected to the following peripherals:

### 1. Actuators & User Interface
* **5x SG90 Servo Motors (Fingers)**: Control five mechanical fingers acting on the piano keys.
  * *Important:* Servos draw high peak current. They must be powered by an external 5V regulated power supply, with the ground shared with the ESP32.
* **1602 LCD Display with I2C Backpack**: Displays current operation status, menus, and the active notes grid.
* **2x Tactile Pushbuttons**:
  * **BTN1 (Mode select)**: Swaps between the 3 modes.
  * **BTN2 (Action/Execute)**: Initiates recording, playback, or stops active operations.

### 2. Audio & Storage Inputs
* **MAX4466 Electret Microphone Amplifier**: Captures analog ambient audio for pitch detection in Mode 2.
* **MicroSD SPI Card Module**: Stores recorded audio and reads pre-recorded `.wav` files in Mode 3.

### 🔌 Pinout Reference Map

| Component | Pin Function | ESP32 GPIO Pin |
| :--- | :--- | :---: |
| **Servo 1 (DO)** | Control signal (PWM) | **GPIO 13** |
| **Servo 2 (RE)** | Control signal (PWM) | **GPIO 14** |
| **Servo 3 (MI)** | Control signal (PWM) | **GPIO 27** |
| **Servo 4 (FA)** | Control signal (PWM) | **GPIO 32** |
| **Servo 5 (SOL)** | Control signal (PWM) | **GPIO 33** |
| **LCD 1602 (I2C)** | SDA (Data Line) | **GPIO 21** |
| **LCD 1602 (I2C)** | SCL (Clock Line) | **GPIO 22** |
| **MicroSD (SPI)** | CS (Chip Select) | **GPIO 5** |
| **MicroSD (SPI)** | SCK (Serial Clock) | **GPIO 18** |
| **MicroSD (SPI)** | MISO (Master In Slave Out) | **GPIO 19** |
| **MicroSD (SPI)** | MOSI (Master Out Slave In) | **GPIO 23** |
| **MAX4466 Mic** | OUT (Analog Signal) | **GPIO 34** (ADC1_CH6) |
| **Button 1** | Switch Mode (Active-LOW, pullup) | **GPIO 25** |
| **Button 2** | Play / Action (Active-LOW, pullup) | **GPIO 26** |

---

## 📂 Project File Guide

### 📦 Root Configuration
* **`platformio.ini`**: The main configuration file for PlatformIO. Defines microcontroller specifications, upload/monitor settings (at 115200 baud), and resolves three primary external library dependencies.

### 🧩 Header Files (`include/`)
* **`include/config.hpp`**: Centralizes all pin configurations, LCD settings, SPI parameters, debouncing timings, and FFT/HPS constants (sampling rate, window frames, thresholds).
* **`include/globals.hpp`**: Holds `extern` declarations for peripheral instances (`degete` servo array, `lcd`), debouncing states, and shared DSP buffers (`gHpsReal`, `gHpsImag`, `gHpsRaw`).
* **`include/helpers.hpp`**: Declares the core `Note` structure, the system `Mode` enumerations, and houses the unified `playNote(const Note&)` finger servo actuation and latch/delay sequence.
* **`include/lcd_utils.hpp`**: Provides LCD display utilities, text-centering helpers (`PrintLinePadded`), and renders the scrolling horizontal notes tracker matrix.
* **`include/audio_dsp.hpp`**: Implements complex math, including building the chromatic notes reference table, a custom radix-2 Cooley-Tukey FFT, the HPS pitch tracking calculations, snapping raw frequencies to the closest note, and duration quantizations.
* **`include/sd_utils.hpp`**: Encapsulates SD card hardware checks, mounts the card SPI bus, defines the 44-byte `.wav` file structure (`WavHeader`), and copies WAV assets from internal SPIFFS flash to the SD card.
* **`include/mode_interface.hpp`**: An abstract base class (`IMode`) defining the uniform API interface (`print()` and `play()`) for all system modes.
* **`include/mode1.hpp`**: Mode 1 implementation. Houses a hardcoded array of 20 notes (`kMode1Notes`) representing a basic melody played sequentially.
* **`include/mode2.hpp`**: Mode 2 implementation. Captures microphone inputs, runs FFT/HPS calculations on-the-fly, maps detected pitches to finger servos, and plays them.
* **`include/mode3.hpp`**: Mode 3 implementation. Reads recorded `.wav` audio files from the SPI MicroSD card, slices files into windows, extracts note sequences, and plays them via the servos.

### 🕹️ Entry Point (`src/`)
* **`src/main.cpp`**: The main entry point. Defines and allocates global buffers, attaches and centers servos in `setup()`, and performs non-blocking hardware button debouncing loop in `loop()` to control mode swapping and playback triggers.

---

## 🕹️ System Operating Modes

The system switches between three modes by pressing **BTN1** (Mode Select), and launches execution by pressing **BTN2** (Play/Action).

### 🎼 Mode 1: Hardcoded Melodies
* Plays a preconfigured, hardcoded melody stored in memory as an array of 20 notes.
* The servo motor mappings associate notes to physical fingers (DO = Servo 0, SOL = Servo 4).
* The LCD prints a scrolling grid tracking which notes are active.

### 🎤 Mode 2: Real-time Audio Detection (MAX4466 Mic)
* Records audio input via the MAX4466 microphone pin at a sample rate of **2048 Hz**.
* Divides the captured time-domain signal into frames of **256 samples** and computes a custom radix-2 Cooley-Tukey FFT.
* Processes the spectrum using the **Harmonic Product Spectrum (HPS)** down-sampling method to eliminate upper harmonic frequencies, pinpointing the true fundamental pitch.
* Automatically maps the fundamental frequency to the closest chromatic musical note, snaps it into the 5 available physical fingers (DO to SOL), and plays it in real-time.

### 💾 Mode 3: SD Card Audio File Reader
* Detects and reads `.wav` files containing standard PCM audio from the SPI MicroSD card.
* Performs full mathematical window analysis frame-by-frame on the file's data payload.
* Extracts the fundamental frequency sequences using the HPS algorithm.
* Matches the extracted frequencies to the target five-note layout, displays the note stream on the LCD screen, and triggers physical key presses on the piano.

---

## 📚 External Libraries Used

The project resolves and installs the following official libraries via PlatformIO:

1. **`kosme/arduinoFFT`** (v2.0.4)
   * *Purpose*: Used as a mathematical reference and tool for complex spectral math where necessary.
2. **`marcoschwartz/LiquidCrystal_I2C`** (v1.1.4)
   * *Purpose*: Direct driver library for controlling the 16x2 Character LCD over the I2C protocol, enabling text layouts, custom characters, and clean buffer output printing.
3. **`madhephaestus/ESP32Servo`** (v3.0.5)
   * *Purpose*: High-accuracy hardware PWM servo motor driver library tailored for the ESP32. Handles servo attachments, positioning, and smooth physical motion angles.

---

## 🚀 Building and Uploading

To build and compile the firmware on your computer, ensure you have **PlatformIO** (either in VS Code or via CLI) installed, then execute:

```bash
# Compile the project
pio run

# Upload the firmware to the connected ESP32
pio run --target upload

# Open the serial communications monitor
pio run --target monitor
```
