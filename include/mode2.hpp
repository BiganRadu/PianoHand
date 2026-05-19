#pragma once
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "mode_interface.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "lcd_utils.hpp"
#include "audio_dsp.hpp"

/**
 * @class Mode2
 * @brief Represents the second mode of operation: live microphone pitch recording and playback.
 * 
 * This mode records audio from the MAX4466 microphone for a defined duration, processes
 * the analog samples in real-time using high-precision Cooley-Tukey FFT and Harmonic Product
 * Spectrum (HPS) pitch detection, filters out transient errors via median filtering, merges
 * contiguous segments, quantizes note durations, and plays back the detected melody on the servos.
 */
class Mode2 : public IMode {
  public:
    /**
     * @brief Prints the Mode 2 menu screen on the LCD display.
     */
    void print() override {
      PrintLinePadded(0, "Mode 2");
      PrintLinePadded(1, "Press to record");
    }

    /**
     * @brief Records audio from the microphone, detects played notes, and plays them back.
     * 
     * Handles the 4 main phases:
     * 1. Countdown and real-time audio sampling + FFT/HPS pitch detection.
     * 2. Median filtering to remove transient spikes/errors.
     * 3. Merging and short segment removal to identify distinct notes.
     * 4. Collecting the melody, quantizing note durations, and playing the sequence on the servos.
     */
    void play() override {
      char prevLine0[17];
      char prevLine1[17];
      memcpy(prevLine0, gLcdLine0, sizeof(prevLine0));
      memcpy(prevLine1, gLcdLine1, sizeof(prevLine1));

      // --- Countdown ---
      PrintLinePadded(0, "Starting...");
      for (int c = 3; c >= 1; --c) {
        char buf[17];
        snprintf(buf, sizeof(buf), "%d", c);
        PrintLinePadded(1, buf);
        delay(1000);
      }

      // --- Phase 1: Record from microphone ---
      PrintLinePadded(0, "Recording...");
      uint32_t totalFrames = (kHpsSampleRate * kMaxRecordSec) / kHpsFftSize;

      // DO=261.6, RE=293.7, MI=329.6, FA=349.2, SOL=392.0
      const float kTargetFreqs[5] = {261.6f, 293.7f, 329.6f, 349.2f, 392.0f};

      struct Frame { int fingerIdx; }; // 0-4 for DO-SOL, -1 for silence
      std::vector<Frame> frames;
      frames.reserve(totalFrames);

      for (uint32_t f = 0; f < totalFrames; ++f) {
        // Update LCD every second
        uint32_t sec = (f * kHpsFftSize) / kHpsSampleRate;
        if (f == 0 || sec != ((f - 1) * kHpsFftSize) / kHpsSampleRate) {
          char timeBuf[17];
          snprintf(timeBuf, sizeof(timeBuf), "%lu/%lu sec", sec, kMaxRecordSec);
          PrintLinePadded(1, timeBuf);
        }

        // Capture and process frame
        captureHpsFrame();

        float energy = 0.0f;
        float sum = 0.0f;
        for (int i = 0; i < kHpsFftSize; ++i) {
          sum += static_cast<float>(gHpsRaw[i]);
        }
        float mean = sum / static_cast<float>(kHpsFftSize);

        for (int i = 0; i < kHpsFftSize; ++i) {
          float s = (static_cast<float>(gHpsRaw[i]) - mean) / 2048.0f;
          energy += s * s;
        }

        if (sqrtf(energy / static_cast<float>(kHpsFftSize)) < kHpsRmsThresh) {
          frames.push_back({-1});
        } else {
          float freq;
          processHpsFrame(&freq, kHpsFftSize, kHpsSampleRate);

          // Direct frequency match against the 5 target notes
          int finger = -1;
          if (freq > 0) {
            float bestDiff = 25.0f; // max tolerance in Hz
            for (int j = 0; j < 5; ++j) {
              float diff = fabsf(freq - kTargetFreqs[j]);
              if (diff < bestDiff) {
                bestDiff = diff;
                finger = j;
              }
            }
            // Also check octave above (freq/2) — harmonics
            if (finger == -1) {
              float halfFreq = freq / 2.0f;
              bestDiff = 20.0f;
              for (int j = 0; j < 5; ++j) {
                float diff = fabsf(halfFreq - kTargetFreqs[j]);
                if (diff < bestDiff) {
                  bestDiff = diff;
                  finger = j;
                }
              }
            }
          }

          frames.push_back({finger});
          Serial.printf("f%02d: %.0fHz -> %s\n", (int)f, freq,
                        finger >= 0 ? kPianoKeys[finger] : "X");
        }
      }

      // --- Phase 2: Median filter ---
      std::vector<int> filtered(frames.size());
      for (size_t i = 0; i < frames.size(); ++i) {
        int counts[6] = {0}; // 0-4 = fingers, 5 = silence
        size_t startIdx = (i > kHpsMedianRadius) ? (i - kHpsMedianRadius) : 0;
        size_t endIdx = std::min(frames.size() - 1, i + (size_t)kHpsMedianRadius);

        for (size_t j = startIdx; j <= endIdx; ++j) {
          int weight = (j == i) ? 3 : 1;
          int idx = frames[j].fingerIdx;
          counts[idx >= 0 ? idx : 5] += weight;
        }

        int bestIdx = 5;
        int bestCount = 0;
        for (int k = 0; k <= 5; ++k) {
          if (counts[k] > bestCount) {
            bestCount = counts[k];
            bestIdx = k;
          }
        }
        filtered[i] = (bestIdx < 5) ? bestIdx : -1;
      }

      // --- Phase 3: Merge segments ---
      struct Segment { int fingerIdx; int startFrame; int endFrame; };
      std::vector<Segment> segments;

      for (size_t i = 0; i < filtered.size(); ++i) {
        if (segments.empty() || segments.back().fingerIdx != filtered[i]) {
          segments.push_back({filtered[i], static_cast<int>(i), static_cast<int>(i)});
        } else {
          segments.back().endFrame = static_cast<int>(i);
        }
      }

      // Remove short segments
      for (auto& seg : segments) {
        if (seg.fingerIdx >= 0 && (seg.endFrame - seg.startFrame + 1) < kHpsMinSegFrames) {
          seg.fingerIdx = -1;
        }
      }

      // Merge again after removal
      std::vector<Segment> mergedSegments;
      for (const auto& seg : segments) {
        if (!mergedSegments.empty() && mergedSegments.back().fingerIdx == seg.fingerIdx) {
          mergedSegments.back().endFrame = seg.endFrame;
        } else {
          mergedSegments.push_back(seg);
        }
      }

      // --- Phase 4: Collect melody + duration filter ---
      std::vector<Note> melody;

      for (const auto& seg : mergedSegments) {
        if (seg.fingerIdx < 0) continue;

        int durationMs = static_cast<int>((seg.endFrame - seg.startFrame + 1) * kHpsFrameMs);
        if (durationMs < kHpsMinDurMs) continue;

        while (durationMs > kHpsMaxDurMs) {
          melody.push_back({kHpsMaxDurMs, seg.fingerIdx, kPianoKeys[seg.fingerIdx]});
          durationMs -= kHpsMaxDurMs;
        }

        if (durationMs >= kHpsMinDurMs) {
          melody.push_back({durationMs, seg.fingerIdx, kPianoKeys[seg.fingerIdx]});
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
