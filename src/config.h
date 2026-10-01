// Pins and fixed settings. Runtime-tunable values (thresholds etc.) do not
// live here.
#pragma once

#include <stdint.h>

// I2C (MPU-6050 at 0x68, BMP388 at 0x77 or 0x76)
constexpr int PIN_I2C_SDA = 21;
constexpr int PIN_I2C_SCL = 22;
constexpr uint32_t I2C_HZ = 400000;

// SPH0645 I2S mic. Board labels: BCLK, LRCL, DOUT. SEL left unconnected.
constexpr int PIN_I2S_SCK = 26;  // BCLK
constexpr int PIN_I2S_WS = 25;   // LRCL
constexpr int PIN_I2S_SD = 33;   // DOUT
constexpr int MIC_SAMPLE_HZ = 16000;

// Green "purring" LED, through a series resistor to GND. GPIO 16 sits next to
// a GND pad on the D1 mini.
constexpr int PIN_LED = 16;

// The MPU's output rate is 1000 / (1 + divider) Hz, so 400 Hz is not
// available. 500 Hz gives plenty of margin over a 20-50 Hz purr.
constexpr int ACCEL_SAMPLE_HZ = 500;

constexpr uint32_t SERIAL_BAUD = 460800;
