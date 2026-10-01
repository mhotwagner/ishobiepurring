// SPH0645 I2S microphone. For now this only measures loudness.
#pragma once

#include <Arduino.h>

// Level reported for a channel that reads as pure digital silence, which
// means nothing is driving the data line.
constexpr float MIC_SILENT_DBFS = -140.0f;

struct MicLevels {
  // RMS level of each I2S channel in dB relative to full scale, with the DC
  // offset removed. The mic drives only one of the two; which index that is
  // depends on the SEL pin and the driver, so both are reported.
  float dbfs[2];
  uint32_t frames;  // audio frames measured
};

bool micBegin();

// Reads whatever audio has arrived, without blocking. Call at least every
// 100 ms or the driver's buffer overruns.
void micPoll();

// Levels over the audio seen since the last call.
MicLevels micTakeLevels();
