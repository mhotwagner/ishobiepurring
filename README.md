# Is Hobie purring?

A small device in Hobie's bed that detects purring and timestamps when it
starts, to answer one question: does he start purring when Michael's head
touches him, earlier when he senses Michael coming, or is he purring the whole
time he is in the bed?

The original project brief is in [docs/kickoff.md](docs/kickoff.md).

## Status

| Milestone | State |
|---|---|
| 1. Bring-up: MPU at a steady rate via FIFO, I2C scan, BMP388, mic levels, LED | firmware runs on the board; no sensors wired yet, so none of the sensor paths have been tested on hardware |
| 2. On-device purr detection with tunable thresholds | not started |
| 3. Wi-Fi, NTP, event transport | not started |
| 4. Approach/contact markers and timeline view | not started |
| 5. Raw-capture debug mode | not started |

## Wiring

ESP32 D1 mini (classic ESP32-WROOM), powered over USB. Everything runs from
the 3V3 pin.

| Module pin | ESP32 pin |
|---|---|
| MPU-6050 VCC / GND | 3V3 / GND |
| MPU-6050 SDA / SCL | GPIO 21 / 22 |
| MPU-6050 AD0 | leave unconnected or GND (address 0x68) |
| MPU-6050 INT (optional, unused so far) | GPIO 27 |
| BMP388 VIN / GND | 3V3 / GND |
| BMP388 SDA (SDI) / SCL (SCK) | GPIO 21 / 22, shared with the MPU |
| SPH0645 3V / GND | 3V3 / GND |
| SPH0645 BCLK / LRCL | GPIO 26 / 25 |
| SPH0645 DOUT | GPIO 33 |
| SPH0645 SEL | leave unconnected |
| Green LED, long leg, through a 330 ohm to 1 k resistor | GPIO 16 |
| Green LED, short leg | GND |

The BMP388 is found at either of its addresses, 0x77 (the Adafruit board's
default) or 0x76. A larger LED resistor gives a dimmer light.

Pins are defined in `src/config.h`.

## Build and flash

Uses [PlatformIO](https://platformio.org/) with the Arduino core. Install the
CLI once with `uv tool install platformio` (or `pipx install platformio`).

```sh
pio run                # compile
pio run -t upload      # compile and flash over USB
pio device monitor     # serial console at 460800 baud, Ctrl-C to quit
```

## Bring-up checks

After flashing, the serial console prints an I2C scan and then one status line
per second:

```
#    12s | accel 500.1 Hz (avg 500.04) fifo max 66 B, overflows 0, i2c errors 0 | mean mg 12 -5 1003 | noise mg 3.10 2.95 4.20 | 27.4 C | bmp 24.10 C 1013.20 hPa | mic dBFS ch0 -78.2 ch1 -140.0 (16000 frames)
```

(The numbers above are illustrative, not measured.)

What to look for:

- **LED** lights for about a second at power-up, and toggles with the `l` key.
- **I2C scan** lists `0x68` (MPU) and `0x77` or `0x76` (BMP388). If it lists
  nothing, check power, SDA and SCL.
- **bmp** shows room temperature and a pressure near 1000 hPa (lower at
  altitude). The temperature rises when you hold a finger on the sensor.
- **accel rate** sits within about 1% of 500 Hz and **overflows** stays at 0.
  The MPU cannot do exactly 400 Hz; its rates are 1000 / (1 + n).
- **mean mg** shows about +/-1000 on the axis that points up through the bed.
  That is the axis purr detection will use.
- **noise mg** rises when you tap the case.
- **mic** shows one channel well above -140 that jumps when you clap. Note
  which channel it is; the other is unused.

Serial commands (single keys):

| Key | Action |
|---|---|
| `s` | toggle raw accelerometer stream as CSV: `seq,ax,ay,az` in raw counts (16384 = 1 g) |
| `l` | toggle the LED |
| `i` | rerun the I2C scan |
| `h` | help |

Every line that is not CSV data starts with `#`.

## Tuning thresholds

Arrives with milestone 2.

## Layout

```
platformio.ini   build config
src/config.h     pins and fixed settings
src/i2c.h        register read/write helpers shared by the I2C drivers
src/mpu.*        MPU-6050 accelerometer FIFO driver (also handles 6500 / 9250)
src/bmp388.*     BMP388 temperature and pressure driver
src/mic.*        SPH0645 I2S mic levels
src/main.cpp     bring-up loop and serial console
docs/            project brief and the case design prompt
```
