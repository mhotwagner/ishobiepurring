#include "mpu.h"

#include "config.h"
#include "i2c.h"

namespace {

constexpr uint8_t MPU_ADDR = 0x68;

// Register addresses are the same on the 6050, 6500 and 9250.
constexpr uint8_t REG_SMPLRT_DIV = 0x19;
constexpr uint8_t REG_CONFIG = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_CONFIG2 = 0x1D;  // 6500 / 9250 only
constexpr uint8_t REG_FIFO_EN = 0x23;
constexpr uint8_t REG_INT_ENABLE = 0x38;
constexpr uint8_t REG_INT_STATUS = 0x3A;
constexpr uint8_t REG_TEMP_OUT_H = 0x41;
constexpr uint8_t REG_USER_CTRL = 0x6A;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_FIFO_COUNT_H = 0x72;
constexpr uint8_t REG_FIFO_R_W = 0x74;
constexpr uint8_t REG_WHO_AM_I = 0x75;

constexpr uint8_t INT_FIFO_OVERFLOW = 0x10;

constexpr size_t BYTES_PER_SAMPLE = 6;
// Wire's receive buffer is 128 bytes, so the FIFO is read 20 samples at a time.
constexpr size_t CHUNK_SAMPLES = 20;

static_assert(1000 % ACCEL_SAMPLE_HZ == 0, "MPU rate must be 1000 / (1 + n)");

// The 6500 and 9250 differ from the 6050 in the accel filter register and the
// temperature formula.
bool is6500Family = false;
char chipName[32] = "none";

uint32_t overflowCount = 0;
uint32_t i2cErrorCount = 0;
uint16_t maxFifoBytes = 0;

bool writeReg(uint8_t reg, uint8_t value) { return i2cWriteReg(MPU_ADDR, reg, value); }

bool readRegs(uint8_t reg, uint8_t *out, size_t len) {
  return i2cReadRegs(MPU_ADDR, reg, out, len);
}

void resetFifo() {
  writeReg(REG_USER_CTRL, 0x00);  // FIFO off
  writeReg(REG_USER_CTRL, 0x04);  // clear it
  writeReg(REG_USER_CTRL, 0x40);  // FIFO on
  uint8_t status;
  readRegs(REG_INT_STATUS, &status, 1);  // reading clears the overflow flag
}

}  // namespace

bool mpuBegin() {
  // Check something answers first; a failed register read makes the Arduino
  // core log an error line.
  if (!i2cPresent(MPU_ADDR)) return false;

  uint8_t who = 0;
  if (!readRegs(REG_WHO_AM_I, &who, 1)) return false;

  is6500Family = (who == 0x70 || who == 0x71 || who == 0x73);
  switch (who) {
    case 0x68: strcpy(chipName, "MPU-6050"); break;
    case 0x70: strcpy(chipName, "MPU-6500"); break;
    case 0x71: strcpy(chipName, "MPU-9250"); break;
    case 0x73: strcpy(chipName, "MPU-9255"); break;
    default:
      // Clones report all sorts of IDs. Treat it as a 6050 and hope.
      snprintf(chipName, sizeof(chipName), "unknown MPU (id 0x%02X)", who);
  }

  writeReg(REG_PWR_MGMT_1, 0x80);  // reset
  delay(100);
  // Wake up and clock from the gyro PLL, which holds the sample rate to about
  // 1%. The internal RC oscillator is several times worse.
  writeReg(REG_PWR_MGMT_1, 0x01);
  delay(50);

  // ~94 Hz low-pass. Passes the purr band and its first harmonics, and keeps
  // anything above Nyquist (250 Hz) from aliasing down into it.
  writeReg(REG_CONFIG, 0x02);
  if (is6500Family) writeReg(REG_ACCEL_CONFIG2, 0x02);
  writeReg(REG_SMPLRT_DIV, 1000 / ACCEL_SAMPLE_HZ - 1);
  writeReg(REG_GYRO_CONFIG, 0x00);
  writeReg(REG_ACCEL_CONFIG, 0x00);  // +/-2 g, the most sensitive range
  writeReg(REG_FIFO_EN, 0x08);       // accel X/Y/Z only
  writeReg(REG_INT_ENABLE, INT_FIFO_OVERFLOW);
  resetFifo();
  return true;
}

const char *mpuChipName() { return chipName; }

size_t mpuReadFifo(AccelSample *out, int64_t *newestMicros) {
  uint8_t status = 0;
  uint8_t countBytes[2];
  if (!readRegs(REG_INT_STATUS, &status, 1) ||
      !readRegs(REG_FIFO_COUNT_H, countBytes, 2)) {
    i2cErrorCount++;
    return 0;
  }
  if (newestMicros) *newestMicros = esp_timer_get_time();

  uint16_t fifoBytes = (countBytes[0] << 8) | countBytes[1];
  if (fifoBytes > maxFifoBytes) maxFifoBytes = fifoBytes;

  if (status & INT_FIFO_OVERFLOW) {
    // Samples were dropped and the X/Y/Z framing can no longer be trusted.
    overflowCount++;
    resetFifo();
    return 0;
  }

  // Rounding down leaves a sample that is still being written for next time.
  size_t total = min((size_t)fifoBytes / BYTES_PER_SAMPLE, MPU_MAX_FIFO_SAMPLES);
  size_t done = 0;
  while (done < total) {
    size_t chunk = min(total - done, CHUNK_SAMPLES);
    uint8_t raw[CHUNK_SAMPLES * BYTES_PER_SAMPLE];
    if (!readRegs(REG_FIFO_R_W, raw, chunk * BYTES_PER_SAMPLE)) {
      i2cErrorCount++;
      resetFifo();
      return 0;
    }
    for (size_t i = 0; i < chunk; i++) {
      const uint8_t *p = raw + i * BYTES_PER_SAMPLE;
      out[done + i].x = (int16_t)((p[0] << 8) | p[1]);
      out[done + i].y = (int16_t)((p[2] << 8) | p[3]);
      out[done + i].z = (int16_t)((p[4] << 8) | p[5]);
    }
    done += chunk;
  }
  return done;
}

float mpuReadTempC() {
  uint8_t raw[2];
  if (!readRegs(REG_TEMP_OUT_H, raw, 2)) return NAN;
  int16_t counts = (int16_t)((raw[0] << 8) | raw[1]);
  return is6500Family ? counts / 333.87f + 21.0f : counts / 340.0f + 36.53f;
}

uint32_t mpuOverflowCount() { return overflowCount; }
uint32_t mpuI2cErrorCount() { return i2cErrorCount; }

uint16_t mpuTakeMaxFifoBytes() {
  uint16_t max = maxFifoBytes;
  maxFifoBytes = 0;
  return max;
}
