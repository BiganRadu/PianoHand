#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "mode_interface.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "lcd_utils.hpp"
#include "sd_utils.hpp"
#include "audio_dsp.hpp"

/**
 * @class Mode3
 * @brief Represents the third mode of operation: WAV file note detection from SD card.
 * 
 * This mode scans the root directory of the SD card for WAV files, presents a file selection
 * menu on the LCD display, reads raw PCM data from the selected WAV file, applies FFT and HPS
 * to compile a note histogram, identifies the top 5 most frequent notes, filters, debounces,
 * and plays the parsed melody back using the servo-actuated physical fingers.
 */
class Mode3 : public IMode {
  public:
    /**
     * @brief Prints the Mode 3 menu screen on the LCD display.
     */
    void print() override {
      PrintLinePadded(0, "Mode 3");
      PrintLinePadded(1, "Press to play");
    }

    /**
     * @brief Reads a selected WAV file from SD card, processes its contents, and plays back the melody.
     * 
     * Handles the following core execution phases:
     * 1. Scans the root directory of the MicroSD card for files ending in ".wav".
     * 2. Displays an interactive file-selection sub-menu mapped to BTN1 (next) and BTN2 (select).
     * 3. Parses the WAV file header, extracts PCM data chunk size, and reads PCM blocks.
     * 4. Converts 8-bit unsigned PCM to normalized signed amplitude blocks, applies FFT/HPS,
     *    and collects a frequency histogram.
     * 5. Selects and sorts the top 5 most prominent unique note pitches in the file.
     * 6. Filters out frequencies outside of the top 5, applies median filtering, merges short
     *    adjacent segments, and quantizes note durations.
     * 7. Plays the compiled melody using the physical servo-controlled finger actuators.
     */
    void play() override {
      char prevLine0[17];
      char prevLine1[17];
      memcpy(prevLine0, gLcdLine0, sizeof(prevLine0));
      memcpy(prevLine1, gLcdLine1, sizeof(prevLine1));

      // --- Scan SD Card for .wav files ---
      std::vector<String> wavFiles;
      File root = SD.open("/");
      if (root) {
        File file = root.openNextFile();
        while (file) {
          if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".wav") || name.endsWith(".WAV")) {
              wavFiles.push_back(String("/") + name);
            }
          }
          file = root.openNextFile();
        }
      }

      if (wavFiles.empty()) {
        PrintLinePadded(0, "No WAV files");
        PrintLinePadded(1, "on SD card");
        delay(2000);
        PrintLinePadded(0, prevLine0);
        PrintLinePadded(1, prevLine1);
        return;
      }

      // --- Sub-menu to select file ---
      int selectedIdx = 0;
      bool fileSelected = false;

      // Ensure buttons are stable before entering loop
      delay(200);

      while (!fileSelected) {
        PrintLinePadded(0, "BTN2 to play:");
        
        // Remove leading '/' for display
        String dispName = wavFiles[selectedIdx];
        if (dispName.startsWith("/")) dispName = dispName.substring(1);
        PrintLinePadded(1, dispName.c_str());

        int b1 = digitalRead(kBtn1Pin);
        int b2 = digitalRead(kBtn2Pin);

        if (b1 == LOW) {
          selectedIdx = (selectedIdx + 1) % wavFiles.size();
          delay(200); // Simple debounce
        }
        if (b2 == LOW) {
          fileSelected = true;
          delay(200); // Simple debounce
        }
        delay(50);
      }

      String targetFile = wavFiles[selectedIdx];
      
      PrintLinePadded(0, "Processing...");
      PrintLinePadded(1, "");

      // --- Open and parse WAV ---
      File wav = SD.open(targetFile.c_str(), FILE_READ);
      if (!wav) {
        PrintLinePadded(0, "Failed to open");
        delay(2000);
        PrintLinePadded(0, prevLine0);
        PrintLinePadded(1, prevLine1);
        return;
      }

      WavHeader header;
      wav.read((uint8_t*)&header, sizeof(header));
      uint32_t totalWavSamples = header.dataChunkSize;
      uint32_t dataStart = wav.position();

      Serial.printf("\nWAV: %s, %luHz, %d-bit, %.1fs\n",
                    targetFile.c_str(), header.sampleRate, header.bitsPerSample,
                    (float)totalWavSamples / header.sampleRate);

      // Mode 3 configuration (matches old WAV logic)
      constexpr uint32_t kWavSampleRate = 8000;
      constexpr int kWavFftSize = 2048;
      constexpr int kWavHopSamples = 200; // 25ms hop
      constexpr float kWavFrameMs = (float)kWavHopSamples / kWavSampleRate * 1000.0f; // 25ms
      
      uint8_t readBuf[kWavFftSize];

      struct Frame { int noteIdx; };
      std::vector<Frame> frames;
      int noteHistogram[60] = {0};

      // --- Phase 1: Process FFT Frames ---
      for (uint32_t pos = 0; pos + kWavFftSize <= totalWavSamples; pos += kWavHopSamples) {
        wav.seek(dataStart + pos);
        size_t rd = wav.read(readBuf, kWavFftSize);
        if ((int)rd < kWavFftSize) break;

        // Convert 8-bit unsigned to int16 (centered, scaled to ~12-bit)
        for (int i = 0; i < kWavFftSize; i++) {
          gHpsRaw[i] = (int16_t)((int)readBuf[i] - 128) * 16;
        }

        // RMS check
        float energy = 0.0f;
        float sum = 0.0f;
        for (int i = 0; i < kWavFftSize; ++i) {
          sum += static_cast<float>(gHpsRaw[i]);
        }
        float mean = sum / static_cast<float>(kWavFftSize);

        for (int i = 0; i < kWavFftSize; ++i) {
          float s = (static_cast<float>(gHpsRaw[i]) - mean) / 2048.0f;
          energy += s * s;
        }

        if (sqrtf(energy / static_cast<float>(kWavFftSize)) < kHpsRmsThresh) {
          frames.push_back({-1});
        } else {
          float freq;
          int noteIdx = processHpsFrame(&freq, kWavFftSize, kWavSampleRate);
          frames.push_back({noteIdx});
          if (noteIdx >= 0) {
            noteHistogram[noteIdx]++;
          }
        }
        
        if (frames.size() % 100 == 0) { yield(); Serial.print("."); }
      }
      wav.close();
      Serial.printf("\nFrames: %d\n", (int)frames.size());

      // --- Phase 2: Histogram -> Top 5 ---
      struct HistEntry { int count; int index; };
      std::vector<HistEntry> sortedEntries;
      for (int i = 0; i < gScaleLen; ++i) {
        if (noteHistogram[i] > 1) {
          sortedEntries.push_back({noteHistogram[i], i});
        }
      }

      std::sort(sortedEntries.begin(), sortedEntries.end(),
                [](const HistEntry& a, const HistEntry& b) { return a.count > b.count; });

      int top5[5] = {-1, -1, -1, -1, -1};
      bool usedBase[12] = {false};
      int numTop = 0;

      for (const auto& entry : sortedEntries) {
        int base = noteBaseIdx(entry.index);
        if (!usedBase[base]) {
          usedBase[base] = true;
          top5[numTop++] = entry.index;
          if (numTop >= 5) break;
        }
      }

      // Sort top 5 by frequency
      std::sort(top5, top5 + numTop, [](int a, int b) {
        return gScale[a].freq < gScale[b].freq;
      });

      Serial.println("Mapare:");
      for (int i = 0; i < numTop; ++i) {
        Serial.printf("  %s -> %s\n", gScale[top5[i]].name, kPianoKeys[i]);
      }

      // --- Phase 3: Filter to top 5 ---
      for (auto& frame : frames) {
        if (frame.noteIdx < 0) continue;
        int base = noteBaseIdx(frame.noteIdx);
        bool valid = false;
        for (int j = 0; j < numTop; ++j) {
          if (noteBaseIdx(top5[j]) == base) {
            valid = true;
            break;
          }
        }
        if (!valid) {
          frame.noteIdx = -1;
        }
      }

      // --- Phase 4: Median filter ---
      std::vector<int> filtered(frames.size());
      for (size_t i = 0; i < frames.size(); ++i) {
        int counts[61] = {0};
        size_t startIdx = (i > kHpsMedianRadius) ? (i - kHpsMedianRadius) : 0;
        size_t endIdx = std::min(frames.size() - 1, i + kHpsMedianRadius);

        for (size_t j = startIdx; j <= endIdx; ++j) {
          int weight = (j == i) ? 3 : 1;
          int idx = frames[j].noteIdx;
          counts[idx >= 0 ? idx : 60] += weight;
        }

        int bestIdx = 60;
        int bestCount = 0;
        for (int k = 0; k <= 60; ++k) {
          if (counts[k] > bestCount) {
            bestCount = counts[k];
            bestIdx = k;
          }
        }
        filtered[i] = (bestIdx < 60) ? bestIdx : -1;
      }

      // --- Phase 5: Merge segments ---
      struct Segment { int noteIdx; int startFrame; int endFrame; };
      std::vector<Segment> segments;

      for (size_t i = 0; i < filtered.size(); ++i) {
        if (segments.empty() || segments.back().noteIdx != filtered[i]) {
          segments.push_back({filtered[i], static_cast<int>(i), static_cast<int>(i)});
        } else {
          segments.back().endFrame = static_cast<int>(i);
        }
      }

      // Remove short segments
      for (auto& seg : segments) {
        if (seg.noteIdx >= 0 && (seg.endFrame - seg.startFrame + 1) < kHpsMinSegFrames) {
          seg.noteIdx = -1;
        }
      }

      // Merge again after removal
      std::vector<Segment> mergedSegments;
      for (const auto& seg : segments) {
        if (!mergedSegments.empty() && mergedSegments.back().noteIdx == seg.noteIdx) {
          mergedSegments.back().endFrame = seg.endFrame;
        } else {
          mergedSegments.push_back(seg);
        }
      }

      // --- Phase 6: Collect melody + duration filter ---
      std::vector<Note> melody;

      for (const auto& seg : mergedSegments) {
        if (seg.noteIdx < 0) continue;

        int durationMs = static_cast<int>((seg.endFrame - seg.startFrame + 1) * kWavFrameMs);
        if (durationMs < kHpsMinDurMs) continue;

        int base = noteBaseIdx(seg.noteIdx);
        int fingerIndex = -1;
        for (int j = 0; j < numTop; ++j) {
          if (noteBaseIdx(top5[j]) == base) {
            fingerIndex = j;
            break;
          }
        }

        if (fingerIndex < 0) continue;

        while (durationMs > kHpsMaxDurMs) {
          melody.push_back({kHpsMaxDurMs, fingerIndex, kPianoKeys[fingerIndex]});
          durationMs -= kHpsMaxDurMs;
        }

        if (durationMs >= kHpsMinDurMs) {
          melody.push_back({durationMs, fingerIndex, kPianoKeys[fingerIndex]});
        }
      }

      // --- Quantize durations ---
      quantizeDurations(melody.data(), melody.size());

      Serial.printf("Notes: %d\n", static_cast<int>(melody.size()));
      for (size_t i = 0; i < melody.size(); ++i) {
        Serial.printf("  %d. %s %dms\n", static_cast<int>(i + 1), melody[i].name.c_str(), melody[i].durationMs);
      }

      if (melody.empty()) {
        PrintLinePadded(0, "No notes found");
        PrintLinePadded(1, "");
        delay(2000);
        PrintLinePadded(0, prevLine0);
        PrintLinePadded(1, prevLine1);
        return;
      }

      // --- Playback ---
      {
        char buf[17];
        snprintf(buf, sizeof(buf), "Got %d notes", static_cast<int>(melody.size()));
        PrintLinePadded(0, "Done!");
        PrintLinePadded(1, buf);
        delay(1500);
      }

      PrintLinePadded(0, "Playing...");
      PrintLinePadded(1, "");

      for (size_t i = 0; i < melody.size(); ++i) {
        int chunkStart = (i / kNoteChunkSize) * kNoteChunkSize;
        char lineBuf[17];
        BuildNoteLine(lineBuf, sizeof(lineBuf), melody.data(), melody.size(), chunkStart, i);

        PrintLinePadded(1, lineBuf);
        playNote(melody[i]);
      }

      PrintLinePadded(0, prevLine0);
      PrintLinePadded(1, prevLine1);
    }
};
