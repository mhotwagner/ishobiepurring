// Small register-access helpers shared by the I2C sensor drivers.
#pragma once

#include <Arduino.h>
#include <Wire.h>

// True if a device answers at `addr`.
inline bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

inline bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// Reads `len` bytes starting at `reg`. Wire's buffer limits len to 128.
inline bool i2cReadRegs(uint8_t addr, uint8_t reg, uint8_t *out, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) out[i] = Wire.read();
  return true;
}
