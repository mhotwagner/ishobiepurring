#include "mic.h"

#include <driver/i2s.h>
#include <soc/i2s_reg.h>

#include "config.h"

namespace {

constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr size_t FRAMES_PER_READ = 256;
constexpr double FULL_SCALE = 8388608.0;  // 2^23, samples are 24-bit

int64_t sum[2] = {0, 0};
int64_t sumSq[2] = {0, 0};
uint32_t frames = 0;

float toDbfs(int64_t sum, int64_t sumSq, uint32_t n) {
  if (n == 0) return MIC_SILENT_DBFS;
  double mean = (double)sum / n;
  double variance = (double)sumSq / n - mean * mean;
  if (variance <= 0) return MIC_SILENT_DBFS;
  return max((float)(10.0 * log10(variance / (FULL_SCALE * FULL_SCALE))),
             MIC_SILENT_DBFS);
}

}  // namespace

bool micBegin() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = MIC_SAMPLE_HZ;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;  // 8 x 256 frames = 128 ms of buffering
  config.dma_buf_len = FRAMES_PER_READ;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_SCK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_SD;

  if (i2s_driver_install(PORT, &config, 0, nullptr) != ESP_OK) return false;

  // The SPH0645 changes its data line later in the clock cycle than the
  // ESP32 expects, so the top bit of every sample gets misread. This is the
  // usual fix: sample the data line a little later.
  REG_SET_FIELD(I2S_TIMING_REG(PORT), I2S_RX_SD_IN_DELAY, 2);
  REG_SET_BIT(I2S_CONF_REG(PORT), I2S_RX_MSB_SHIFT);

  if (i2s_set_pin(PORT, &pins) != ESP_OK) return false;
  return true;
}

void micPoll() {
  static int32_t buffer[FRAMES_PER_READ * 2];
  for (;;) {
    size_t bytesRead = 0;
    i2s_read(PORT, buffer, sizeof(buffer), &bytesRead, 0);
    size_t n = bytesRead / (2 * sizeof(int32_t));
    if (n == 0) return;
    for (size_t i = 0; i < n; i++) {
      for (int ch = 0; ch < 2; ch++) {
        // The sample (18 significant bits) sits in the top of the 32-bit slot.
        int64_t s = buffer[i * 2 + ch] >> 8;
        sum[ch] += s;
        sumSq[ch] += s * s;
      }
    }
    frames += n;
  }
}

MicLevels micTakeLevels() {
  MicLevels levels;
  for (int ch = 0; ch < 2; ch++) {
    levels.dbfs[ch] = toDbfs(sum[ch], sumSq[ch], frames);
    sum[ch] = sumSq[ch] = 0;
  }
  levels.frames = frames;
  frames = 0;
  return levels;
}
