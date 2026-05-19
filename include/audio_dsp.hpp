#pragma once
#include <Arduino.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include "helpers.hpp"
#include "config.hpp"
#include "globals.hpp"

/**
 * @brief Represents a chromatic note entry with its friendly name and reference frequency.
 */
struct NoteEntry {
    char name[8]; ///< Note name string including octave (e.g. "DO3")
    float freq;   ///< Note fundamental frequency in Hz
};

/// Chromatic scale notes naming templates within a single octave
inline const char* const kNoteNamesChrom[] = {"DO","DO#","RE","RE#","MI","FA","FA#","SOL","SOL#","LA","LA#","SI"};

/// Array storing the built chromatic lookup scale (C2 to C6)
inline NoteEntry gScale[60];

/// Current count of compiled notes inside the chromatic gScale array
inline int gScaleLen = 0;

/**
 * @brief Builds the chromatic lookup scale (C2 to C6) using Equal Temperament formula 
 *        referenced to A4 = 440 Hz.
 */
inline void buildChromaticScale() {
  gScaleLen = 0;
  for (int o = 2; o <= 6; o++) {
    for (int s = 0; s < 12; s++) {
      float f = 440.0f * powf(2.0f, ((o+1)*12+s - 69) / 12.0f);
      snprintf(gScale[gScaleLen].name, 8, "%s%d", kNoteNamesChrom[s], o);
      gScale[gScaleLen++].freq = f;
    }
  }
}

/**
 * @brief Finds the closest chromatic note in gScale matching a target frequency
 *        within a strict musical cents threshold limit.
 * 
 * @param freq Target frequency to test.
 * @return Index in gScale or -1 if no matching note was found.
 */
inline int findClosestNote(float freq) {
  if (freq < 60 || freq > 2000) return -1;
  float bd = 100; int bi = -1;
  for (int i = 0; i < gScaleLen; i++) {
    float d = fabsf(12.0f * log2f(freq / gScale[i].freq));
    if (d < bd) { bd = d; bi = i; }
  }
  return (bd > 0.75f) ? -1 : bi;
}

/**
 * @brief Calculates the base pitch class index (0 to 11) within an octave (DO to SI).
 * 
 * @param i Index in the chromatic gScale.
 * @return Base pitch class index.
 */
inline int noteBaseIdx(int i) {
    return i % 12;
}

/**
 * @brief Cooley-Tukey Radix-2 Fast Fourier Transform (FFT) in-place implementation.
 * 
 * @param N FFT size (must be power of 2).
 */
inline void hpsFftCompute(int N) {
  for (int i = 1, j = 0; i < N; i++) {
    int bit = N >> 1;
    while (j & bit) { j ^= bit; bit >>= 1; }
    j ^= bit;
    if (i < j) { std::swap(gHpsReal[i], gHpsReal[j]); std::swap(gHpsImag[i], gHpsImag[j]); }
  }
  for (int len = 2; len <= N; len <<= 1) {
    float ang = -2.0f * PI / len, wR = cosf(ang), wI = sinf(ang);
    for (int i = 0; i < N; i += len) {
      float cR = 1, cI = 0;
      for (int j = 0; j < len/2; j++) {
        float uR=gHpsReal[i+j], uI=gHpsImag[i+j];
        float vR=gHpsReal[i+j+len/2]*cR - gHpsImag[i+j+len/2]*cI;
        float vI=gHpsReal[i+j+len/2]*cI + gHpsImag[i+j+len/2]*cR;
        gHpsReal[i+j]=uR+vR; gHpsImag[i+j]=uI+vI;
        gHpsReal[i+j+len/2]=uR-vR; gHpsImag[i+j+len/2]=uI-vI;
        float t=cR*wR-cI*wI; cI=cR*wI+cI*wR; cR=t;
      }
    }
  }
}

/**
 * @brief Uses Harmonic Product Spectrum (HPS) to find the fundamental frequency
 *        of an audio signal inside the FFT real/imag buffers.
 * 
 * @param minF Minimum target frequency.
 * @param maxF Maximum target frequency.
 * @param fftSize Size of the FFT.
 * @param sampleRate Sampling rate.
 * @return Detected fundamental frequency in Hz, or 0 if below threshold.
 */
inline float findFundamentalHPS(float minF, float maxF, int fftSize, uint32_t sampleRate) {
  float res = (float)sampleRate / fftSize;
  int half = fftSize / 2;
  for (int i = 0; i < half; i++)
    gHpsReal[i] = sqrtf(gHpsReal[i]*gHpsReal[i] + gHpsImag[i]*gHpsImag[i]);
  for (int i = 0; i < half; i++)
    gHpsImag[i] = (gHpsReal[i] > 1e-6f) ? logf(gHpsReal[i]) : -14.0f;
  for (int i = 0; i < half/2; i++) {
    float v = (gHpsReal[i*2] > 1e-6f) ? logf(gHpsReal[i*2]) : -14.0f;
    gHpsImag[i] += v;
  }
  int lo = std::max(1,(int)(minF/res)), hi = std::min((int)(maxF/res), half/kHpsHarmonics-1);
  int best = lo; float bv = -1e9f;
  for (int i = lo; i <= hi; i++) if (gHpsImag[i] > bv) { bv = gHpsImag[i]; best = i; }
  if (bv < -14.0f * kHpsHarmonics) return 0;
  float freq = (float)best * res;
  if (best > lo && best < hi) {
    float a=gHpsImag[best-1], b=gHpsImag[best], c=gHpsImag[best+1], dn=2*(a-2*b+c);
    if (fabsf(dn) > 1e-6f) freq = ((float)best + (a-c)/dn) * res;
  }
  return freq;
}

/**
 * @brief Captures a block of samples from the microphone at a exact sample rate
 *        regulated using high-precision microseconds delay.
 */
inline void captureHpsFrame() {
  uint32_t ns = micros();
  for (int i = 0; i < kHpsFftSize; i++) {
    while ((int32_t)(micros()-ns) < 0) {} ns += kHpsSamplePeriodUs;
    gHpsRaw[i] = (int16_t)analogRead(kMicPin);
  }
}

/**
 * @brief Processes an audio buffer (DC offset removal, Hanning windowing,
 *        Cooley-Tukey FFT, and HPS fundamental mapping).
 * 
 * @param outF Destination float pointer to store the fundamental frequency.
 * @param fftSize The FFT size.
 * @param sampleRate Sampling rate.
 * @return Mapped chromatic scale index in gScale, or -1 if silent/invalid.
 */
inline int processHpsFrame(float *outF, int fftSize, uint32_t sampleRate) {
  float sum = 0;
  for (int i = 0; i < fftSize; i++) sum += (float)gHpsRaw[i];
  float mean = sum / fftSize;
  for (int i = 0; i < fftSize; i++) {
    float w = 0.5f*(1.0f-cosf(2.0f*PI*i/(fftSize-1)));
    gHpsReal[i] = ((float)gHpsRaw[i]-mean)*w; gHpsImag[i] = 0;
  }
  hpsFftCompute(fftSize);
  float f = findFundamentalHPS(100.0f, 800.0f, fftSize, sampleRate);
  *outF = f;
  return (f > 0) ? findClosestNote(f) : -1;
}

/**
 * @brief Quantizes note durations, snapping them to musical targets (750ms, 1000ms, or 1250ms)
 *        for consistent rhythmic playback.
 * 
 * @param notes Pointer to the array of Note elements.
 * @param count Number of notes in the array.
 */
inline void quantizeDurations(Note* notes, int count) {
  if (count == 0) return;
  int minD = 99999, maxD = 0;
  for (int i = 0; i < count; i++) {
    if (notes[i].durationMs < minD) minD = notes[i].durationMs;
    if (notes[i].durationMs > maxD) maxD = notes[i].durationMs;
  }
  const int targets[] = {750, 1000, 1250};
  for (int i = 0; i < count; i++) {
    float scaled;
    if (maxD == minD) scaled = 1000.0f;
    else scaled = 750.0f + (float)(notes[i].durationMs - minD) / (maxD - minD) * 500.0f;
    int best = 750, bd = 9999;
    for (int t = 0; t < 3; t++) {
      int d = std::abs((int)scaled - targets[t]);
      if (d < bd) { bd = d; best = targets[t]; }
    }
    notes[i].durationMs = best;
  }
}

/**
 * @brief Performs Peak-to-Peak ADC reading of the microphone to measure amplitude/noise.
 * 
 * @param samples Number of ADC readings to sample.
 * @param delayUs Delay between each reading in microseconds.
 * @return The difference between highest and lowest sampled values.
 */
inline uint16_t ReadMicPeakToPeak(uint16_t samples, uint16_t delayUs) {
  uint16_t minVal = 4095;
  uint16_t maxVal = 0;

  for (uint16_t i = 0; i < samples; ++i) {
    uint16_t value = analogRead(kMicPin);
    if (value < minVal) {
      minVal = value;
    }
    if (value > maxVal) {
      maxVal = value;
    }
    if (delayUs > 0) {
      delayMicroseconds(delayUs);
    }
  }

  return maxVal - minVal;
}
