// Is Hobie purring? -- milestone 1: bring-up.
//
// Checks that the sensors (MPU-6050, BMP388, SPH0645 mic) and the LED are
// alive and that the accelerometer delivers a steady 500 Hz. Prints one
// status line per second, and can stream raw accelerometer samples as CSV.
// Every line that is not CSV data starts with '#'.

#include <Arduino.h>
#include <Wire.h>

#include "bmp388.h"
#include "config.h"
#include "mic.h"
#include "mpu.h"

constexpr uint32_t STATUS_INTERVAL_MS = 1000;
constexpr uint32_t SENSOR_RETRY_INTERVAL_MS = 2000;

static bool mpuReady = false;
static bool bmpReady = false;
static bool micReady = false;
static bool streaming = false;
static bool ledOn = false;

static uint32_t totalSamples = 0;  // also the sequence number in the CSV stream

// Accumulated over one status interval, in raw counts.
static int64_t axisSum[3];
static int64_t axisSumSq[3];
static uint32_t statSamples = 0;

// A point on the sample clock: `samples` had been read by time `micros`.
// The measured sample rate is the slope between two of these.
struct RatePoint {
  int64_t micros;
  uint32_t samples;
};
static RatePoint rateStart, rateWindowStart, rateNow;
static bool rateValid = false;
static uint32_t lastFaultCount = 0;

static float rateBetween(const RatePoint &from, const RatePoint &to) {
  if (to.micros <= from.micros) return 0;
  return (to.samples - from.samples) * 1e6f / (to.micros - from.micros);
}

static void scanI2c() {
  Serial.println("# I2C scan:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) continue;
    const char *guess = addr == 0x68                   ? " (MPU)"
                        : addr == 0x76 || addr == 0x77 ? " (BMP388?)"
                                                       : "";
    Serial.printf("#   0x%02X%s\n", addr, guess);
    found++;
  }
  if (found == 0) Serial.println("#   nothing found -- check wiring and power");
}

// The start functions are retried until the sensor appears, so they report
// a missing sensor only once.
static void startMpu() {
  static bool saidMissing = false;
  mpuReady = mpuBegin();
  rateValid = false;
  if (mpuReady) {
    Serial.printf("# %s found, sampling at %d Hz\n", mpuChipName(), ACCEL_SAMPLE_HZ);
    saidMissing = false;
  } else if (!saidMissing) {
    Serial.println("# no MPU at 0x68, will keep retrying");
    saidMissing = true;
  }
}

static void startBmp() {
  static bool saidMissing = false;
  bmpReady = bmpBegin();
  if (bmpReady) {
    Serial.printf("# BMP388 found at 0x%02X\n", bmpAddress());
    saidMissing = false;
  } else if (!saidMissing) {
    Serial.println("# no BMP388 at 0x77 or 0x76, will keep retrying");
    saidMissing = true;
  }
}

static void setLed(bool on) {
  ledOn = on;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

static void printHelp() {
  Serial.println("# commands: s = toggle raw CSV stream, l = toggle LED, i = I2C scan, h = help");
}

static void handleSerial() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 's':
        streaming = !streaming;
        if (streaming) {
          Serial.printf("# stream on: seq,ax,ay,az (raw counts, %d = 1 g, %d Hz)\n",
                        (int)ACCEL_COUNTS_PER_G, ACCEL_SAMPLE_HZ);
        } else {
          Serial.println("# stream off");
        }
        break;
      case 'l':
        setLed(!ledOn);
        Serial.printf("# LED %s\n", ledOn ? "on" : "off");
        break;
      case 'i':
        scanI2c();
        break;
      case 'h':
      case '?':
        printHelp();
        break;
    }
  }
}

static void pollAccel() {
  static AccelSample samples[MPU_MAX_FIFO_SAMPLES];
  int64_t drainMicros = 0;
  size_t n = mpuReadFifo(samples, &drainMicros);

  // An overflow or I2C error drops samples, which would skew the rate.
  uint32_t faultCount = mpuOverflowCount() + mpuI2cErrorCount();
  if (faultCount != lastFaultCount) {
    lastFaultCount = faultCount;
    rateValid = false;
  }
  if (n == 0) return;

  for (size_t i = 0; i < n; i++) {
    const int16_t axes[3] = {samples[i].x, samples[i].y, samples[i].z};
    for (int a = 0; a < 3; a++) {
      axisSum[a] += axes[a];
      axisSumSq[a] += (int32_t)axes[a] * axes[a];
    }
    if (streaming) {
      Serial.printf("%u,%d,%d,%d\n", totalSamples + i, axes[0], axes[1], axes[2]);
    }
  }
  totalSamples += n;
  statSamples += n;

  rateNow = {drainMicros, totalSamples};
  if (!rateValid) {
    rateStart = rateWindowStart = rateNow;
    rateValid = true;
  }
}

static void printStatus() {
  Serial.printf("# %5lus", millis() / 1000);

  if (!mpuReady) {
    Serial.print(" | accel: no MPU");
  } else if (statSamples == 0) {
    Serial.print(" | accel: no samples");
  } else {
    Serial.printf(" | accel %.1f Hz (avg %.2f) fifo max %u B, overflows %u, i2c errors %u",
                  rateBetween(rateWindowStart, rateNow), rateBetween(rateStart, rateNow),
                  mpuTakeMaxFifoBytes(), mpuOverflowCount(), mpuI2cErrorCount());

    // Mean shows which way gravity points; noise is the standard deviation,
    // i.e. how much each axis is shaking.
    float meanMg[3], noiseMg[3];
    for (int a = 0; a < 3; a++) {
      double mean = (double)axisSum[a] / statSamples;
      double variance = (double)axisSumSq[a] / statSamples - mean * mean;
      meanMg[a] = mean / ACCEL_COUNTS_PER_G * 1000;
      noiseMg[a] = sqrt(max(variance, 0.0)) / ACCEL_COUNTS_PER_G * 1000;
    }
    Serial.printf(" | mean mg %.0f %.0f %.0f | noise mg %.2f %.2f %.2f | %.1f C",
                  meanMg[0], meanMg[1], meanMg[2], noiseMg[0], noiseMg[1], noiseMg[2],
                  mpuReadTempC());
  }
  rateWindowStart = rateNow;
  for (int a = 0; a < 3; a++) axisSum[a] = axisSumSq[a] = 0;
  statSamples = 0;

  float bmpTempC, bmpHpa;
  if (!bmpReady) {
    Serial.print(" | bmp: none");
  } else if (bmpRead(&bmpTempC, &bmpHpa)) {
    Serial.printf(" | bmp %.2f C %.2f hPa", bmpTempC, bmpHpa);
  } else {
    Serial.print(" | bmp: read failed");
    bmpReady = false;  // unplugged? fall back to retrying
  }

  if (!micReady) {
    Serial.print(" | mic: init failed");
  } else {
    MicLevels mic = micTakeLevels();
    Serial.printf(" | mic dBFS ch0 %.1f ch1 %.1f (%u frames)", mic.dbfs[0], mic.dbfs[1],
                  mic.frames);
  }
  Serial.println();
}

void setup() {
  // Lamp test: the LED stays on through start-up, about a second. The wait
  // happens before the sensors start so their buffers do not fill meanwhile.
  pinMode(PIN_LED, OUTPUT);
  setLed(true);

  Serial.begin(SERIAL_BAUD);
  delay(800);
  Serial.println("\n# Is Hobie purring? -- bring-up");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_HZ);
  scanI2c();
  startMpu();
  startBmp();

  micReady = micBegin();
  if (!micReady) Serial.println("# I2S mic init failed");

  printHelp();
  setLed(false);
}

void loop() {
  static uint32_t lastStatusMs = 0;
  static uint32_t lastRetryMs = 0;
  uint32_t now = millis();

  if (mpuReady) pollAccel();
  if ((!mpuReady || !bmpReady) && now - lastRetryMs >= SENSOR_RETRY_INTERVAL_MS) {
    lastRetryMs = now;
    if (!mpuReady) startMpu();
    if (!bmpReady) startBmp();
  }

  if (micReady) micPoll();
  handleSerial();

  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    printStatus();
  }

  delay(10);
}
