# Project: "Is Hobie purring?" — purr onset monitor (MVP)

## Goal
Build firmware (and a minimal data path) for a small device embedded in my cat Hobie's bed that detects when he is purring and timestamps purr onset precisely.

The real question I want answered: **does Hobie start purring (a) the instant my head touches him, (b) earlier, when he senses me approaching (sound, smell, airflow), or (c) is he already purring whenever he's in the bed?** So the core requirement is accurate, timestamped purr start/stop events, plus an independent marker for "Michael approaching / making contact."

## Hardware (fixed, already chosen)
- **MCU:** ESP32 D1 mini, USB-C, classic ESP32-WROOM (confirm the variant from the board markings; if it's an S2 or C3 mini, remap pins). USB-powered, no battery in the MVP.
- **Accelerometer:** MPU-6050 (or MPU-9250/6500, I have both), I2C at 0x68. This is the primary purr sensor. It's mounted rigidly to the case floor under the cushion where he lies.
- **Mic:** INMP441, I2S. Secondary signal: purr audio, plus incidental "human approaching" sounds.
- **Temp (optional):** BMP280 on I2C at 0x76 if I add it. Otherwise use the MPU's internal temp register as a rough "bed is warm, cat present" signal.

### Pinout
| Function | GPIO |
|---|---|
| I2C SDA / SCL | 21 / 22 |
| MPU INT (motion interrupt, optional) | 27 |
| I2S SCK / WS | 26 / 25 |
| I2S SD (mic data in) | 33 |
| INMP441 L/R | tied to GND (left channel) |

## Signal notes
- A cat purr fundamental is roughly **20–30 Hz** (sometimes up to ~50 Hz), with harmonics. It's continuous through both inhale and exhale, with a slight break at the transitions.
- Sample the accelerometer at **≥200 Hz, ideally 400 Hz**, using the FIFO so sampling isn't jittered by Wi-Fi. Use the axis normal to the bed, or the vector magnitude.
- The detection approach is up to you. Start simple: band-pass around 20–35 Hz, compute RMS energy over a ~0.5 s window, and apply hysteresis plus a minimum duration (e.g. ≥1.5 s) to declare onset or offset. A Goertzel filter or small FFT on the 20–30 Hz bins is a fine alternative.
- Thresholds must be tunable at runtime, not just compile-time, because I'll calibrate on the real cat.
- The mic is secondary. It's useful for confirming purr and for logging loudness and onset of non-purr sound as a crude "someone's here" signal. Don't block the MVP on it.

## Data and events
- Sync time over NTP. Every event needs a wall-clock timestamp with ~100 ms accuracy or better.
- Events to emit: `cat_present` / `cat_left` (from motion and temp), `purr_start` / `purr_stop` (with confidence and peak energy), and periodic low-rate telemetry (energy level, temp, heartbeat).
- **Approach marker.** I need an independent timestamp for my approach and contact. For the MVP, provide a simple way for me to log "approaching" and "contact" with one tap (a tiny web page served by the ESP32, or an endpoint the app hits). The mic loudness spike is a bonus secondary marker.
- **Debug capture.** Make it possible to grab a few seconds of raw accelerometer data (and ideally audio) around a purr_start, so I can tune thresholds offline.

## Open decisions (propose, briefly justify, then proceed)
1. **Firmware framework.** Arduino core via PlatformIO vs. ESP-IDF vs. something else. My leaning is PlatformIO + Arduino for speed, unless you see a real reason to go IDF (e.g. I2S plus FIFO timing).
2. **Transport and backend.** How events get off the device. Options: MQTT to a local broker, HTTP POST to a small server, or the device serving its own page or API. My stack is Python/Django/Postgres/React, so lean that way for anything server-side, but keep it minimal.
3. **App or UI.** The minimum viable view is a timeline of purr spans overlaid with my approach and contact markers, so I can eyeball onset vs. contact. A simple web page is fine; no native app for the MVP.

## Milestones
1. Bring-up: read the MPU at a stable 400 Hz via FIFO and print or stream it. Confirm I2C devices and I2S mic samples.
2. On-device purr detection with tunable thresholds, logged over serial.
3. Wi-Fi + NTP + event transport.
4. Approach/contact marker button and timeline view.
5. Raw-capture debug mode for tuning.

## Constraints and style
- Keep it simple and readable; this is a hobby project I'll be hacking on.
- Wi-Fi credentials and config go in a gitignored config file, not hardcoded.
- Write a short README with wiring, build/flash steps, and how to tune thresholds.
- Ask before adding hardware or big dependencies. **Out of scope for now:** battery/power management, deep sleep, and turning this into a general cat monitor (that's v2).

## Decisions made since

- **Framework:** PlatformIO + Arduino core, plain `.cpp`/`.h` files (2026-10-01).
- **Accelerometer rate:** 500 Hz, because the MPU only offers 1000 / (1 + n) Hz, so exactly 400 Hz is not available (2026-10-01).
- **Hardware changes** (2026-10-01): the mic is an SPH0645 instead of the INMP441, the temperature sensor is a BMP388 instead of the BMP280, the accelerometer is the MPU-6050, and a green LED lights while Hobie purrs. The ESP32 has no pin headers. The LED is on GPIO 16. Bring-up firmware supports all of these but none has been tested on real sensors yet.
- **Case:** designed in Claude Design from the prompt in [case-design-prompt.md](case-design-prompt.md) (2026-10-01).
