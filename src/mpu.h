// Minimal MPU-6050 / MPU-6500 / MPU-9250 driver: accelerometer only, read
// through the chip's FIFO so the sample timing is set by the MPU's own clock
// and not by how busy the ESP32 is.
#pragma once

#include <Arduino.h>

struct AccelSample {
  int16_t x, y, z;  // raw counts, +/-2 g full scale
};

constexpr float ACCEL_COUNTS_PER_G = 16384.0f;

// The FIFO holds at most 1024 bytes (MPU-6050) = 170 samples.
constexpr size_t MPU_MAX_FIFO_SAMPLES = 170;

// Resets and configures the chip. Returns false if nothing answers at 0x68.
bool mpuBegin();

// Name of the detected chip, e.g. "MPU-6050".
const char *mpuChipName();

// Moves everything waiting in the FIFO into `out` (which must hold
// MPU_MAX_FIFO_SAMPLES) and returns the number of samples, oldest first.
// If `newestMicros` is given it is set to the time the FIFO level was read,
// which is the time of the last sample returned, to within one sample period.
size_t mpuReadFifo(AccelSample *out, int64_t *newestMicros = nullptr);

// Die temperature in Celsius. Rough, but tracks "the bed is warm".
float mpuReadTempC();

// Diagnostics since boot.
uint32_t mpuOverflowCount();  // FIFO filled up before we drained it
uint32_t mpuI2cErrorCount();
uint16_t mpuTakeMaxFifoBytes();  // high-water mark since the last call
