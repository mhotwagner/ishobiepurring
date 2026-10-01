#include "bmp388.h"

#include "i2c.h"

namespace {

constexpr uint8_t REG_CHIP_ID = 0x00;
constexpr uint8_t REG_DATA = 0x04;  // pressure (3 bytes) then temperature (3)
constexpr uint8_t REG_PWR_CTRL = 0x1B;
constexpr uint8_t REG_OSR = 0x1C;
constexpr uint8_t REG_ODR = 0x1D;
constexpr uint8_t REG_CONFIG = 0x1F;
constexpr uint8_t REG_CALIB = 0x31;
constexpr uint8_t REG_CMD = 0x7E;

constexpr uint8_t CHIP_ID_BMP388 = 0x50;
constexpr uint8_t CHIP_ID_BMP390 = 0x60;
constexpr uint8_t CMD_SOFT_RESET = 0xB6;

uint8_t address = 0;

// Factory calibration, scaled to floating point as in Bosch's reference
// driver (BMP3_SensorAPI, parse_calib_data).
struct Calibration {
  double t1, t2, t3;
  double p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11;
} cal;

bool readCalibration() {
  uint8_t d[21];
  if (!i2cReadRegs(address, REG_CALIB, d, sizeof(d))) return false;

  auto u16 = [&](int i) { return (uint16_t)(d[i] | (d[i + 1] << 8)); };
  auto s16 = [&](int i) { return (int16_t)u16(i); };
  auto s8 = [&](int i) { return (int8_t)d[i]; };

  cal.t1 = u16(0) * 256.0;                // / 2^-8
  cal.t2 = u16(2) / 1073741824.0;         // / 2^30
  cal.t3 = s8(4) / 281474976710656.0;     // / 2^48
  cal.p1 = (s16(5) - 16384) / 1048576.0;  // / 2^20
  cal.p2 = (s16(7) - 16384) / 536870912.0;  // / 2^29
  cal.p3 = s8(9) / 4294967296.0;            // / 2^32
  cal.p4 = s8(10) / 137438953472.0;         // / 2^37
  cal.p5 = u16(11) * 8.0;                   // / 2^-3
  cal.p6 = u16(13) / 64.0;
  cal.p7 = s8(15) / 256.0;
  cal.p8 = s8(16) / 32768.0;
  cal.p9 = s16(17) / 281474976710656.0;        // / 2^48
  cal.p10 = s8(19) / 281474976710656.0;        // / 2^48
  cal.p11 = s8(20) / 36893488147419103232.0;   // / 2^65
  return true;
}

}  // namespace

bool bmpBegin() {
  address = 0;
  for (uint8_t candidate : {0x77, 0x76}) {
    uint8_t id = 0;
    if (!i2cPresent(candidate) || !i2cReadRegs(candidate, REG_CHIP_ID, &id, 1)) continue;
    if (id == CHIP_ID_BMP388 || id == CHIP_ID_BMP390) {
      address = candidate;
      break;
    }
  }
  if (address == 0) return false;

  i2cWriteReg(address, REG_CMD, CMD_SOFT_RESET);
  delay(10);
  if (!readCalibration()) return false;

  // Pressure oversampled x8, temperature x1, a new reading every 80 ms,
  // lightly smoothed. Far more than a once-a-second poll needs.
  i2cWriteReg(address, REG_OSR, 0x03);
  i2cWriteReg(address, REG_ODR, 0x04);     // 12.5 Hz
  i2cWriteReg(address, REG_CONFIG, 0x04);  // IIR filter coefficient 3
  // Pressure on, temperature on, normal (continuous) mode.
  return i2cWriteReg(address, REG_PWR_CTRL, 0x33);
}

uint8_t bmpAddress() { return address; }

bool bmpRead(float *tempC, float *pressureHpa) {
  uint8_t d[6];
  if (address == 0 || !i2cReadRegs(address, REG_DATA, d, sizeof(d))) return false;
  double rawP = (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16);
  double rawT = (uint32_t)d[3] | ((uint32_t)d[4] << 8) | ((uint32_t)d[5] << 16);

  // Compensation formulas from the BMP388 datasheet, section 9.
  double dt = rawT - cal.t1;
  double t = dt * cal.t2 + dt * dt * cal.t3;

  double offset = cal.p5 + cal.p6 * t + cal.p7 * t * t + cal.p8 * t * t * t;
  double scale = cal.p1 + cal.p2 * t + cal.p3 * t * t + cal.p4 * t * t * t;
  double pascals = offset + rawP * scale + rawP * rawP * (cal.p9 + cal.p10 * t) +
                   rawP * rawP * rawP * cal.p11;

  *tempC = t;
  *pressureHpa = pascals / 100.0;
  return true;
}
