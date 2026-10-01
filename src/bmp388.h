// Minimal BMP388 / BMP390 temperature and pressure driver.
#pragma once

#include <Arduino.h>

// Looks for the sensor at 0x77 (Adafruit's default) and then 0x76, and starts
// continuous measurement. Returns false if neither address holds a BMP388/390.
bool bmpBegin();

// I2C address the sensor was found at.
uint8_t bmpAddress();

// Latest reading. Returns false on an I2C error.
bool bmpRead(float *tempC, float *pressureHpa);
