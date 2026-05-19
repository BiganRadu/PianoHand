#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include "config.hpp"
#include "globals.hpp"

/**
 * @brief Structure representing the standard RIFF/WAV file header format (44 bytes).
 */
struct WavHeader {
  char riff[4]; uint32_t fileSizeMinus8; char wave[4];
  char fmtChunkId[4]; uint32_t fmtChunkSize;
  uint16_t audioFormat; uint16_t numChannels;
  uint32_t sampleRate; uint32_t byteRate;
  uint16_t blockAlign; uint16_t bitsPerSample;
  char dataChunkId[4]; uint32_t dataChunkSize;
};

/**
 * @brief Mounts, initializes, and tests the MicroSD card reader via the SPI bus.
 *        Performs diagnostic reads and writes to guarantee SD integrity.
 * 
 * @return True if SD initialization and self-test are successful.
 */
inline bool InitAndTestSd() {
  SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
  if (!SD.begin(kSdCs, SPI)) {
    Serial.println("SD init failed");
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card detected");
    return false;
  }

  uint64_t cardSizeMb = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD card OK, size: %llu MB\n", cardSizeMb);

  File file = SD.open("/sd_test.txt", FILE_APPEND);
  if (!file) {
    Serial.println("SD open failed");
    return false;
  }
  file.println("boot");
  file.close();

  File readFile = SD.open("/sd_test.txt", FILE_READ);
  if (!readFile) {
    Serial.println("SD read failed");
    return false;
  }
  readFile.close();

  return true;
}

/**
 * @brief Copies a file from the SPIFFS (ESP32 internal flash file system) to the MicroSD card.
 * 
 * @param spiffsPath Absolute source path in SPIFFS.
 * @param sdPath Absolute destination path on SD Card.
 * @return True if the copy completed successfully.
 */
inline bool CopySpiffsFileToSd(const char *spiffsPath, const char *sdPath) {
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS mount failed");
    return false;
  }
  if (!SPIFFS.exists(spiffsPath)) {
    Serial.println("SPIFFS file missing");
    return false;
  }

  File src = SPIFFS.open(spiffsPath, FILE_READ);
  if (!src) {
    Serial.println("SPIFFS open failed");
    return false;
  }

  if (SD.exists(sdPath)) {
    SD.remove(sdPath);
  }

  File dst = SD.open(sdPath, FILE_WRITE);
  if (!dst) {
    Serial.println("SD write open failed");
    src.close();
    return false;
  }

  uint8_t buffer[512];
  while (src.available()) {
    size_t readBytes = src.read(buffer, sizeof(buffer));
    if (readBytes == 0) {
      break;
    }
    size_t written = dst.write(buffer, readBytes);
    if (written != readBytes) {
      Serial.println("SD write failed");
      src.close();
      dst.close();
      return false;
    }
  }

  src.close();
  dst.close();
  return true;
}
