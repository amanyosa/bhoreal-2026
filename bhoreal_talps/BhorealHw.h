// Bhoreal SLIM hardware: 8x8 WS2812 LEDs on pin 6 and an 8x8 button matrix
// scanned through a 74HC164 (rows) and a 74HC165 (columns).
// Pin usage and timings come from the original Bhoreal SLIM firmware.
#pragma once
#include <Arduino.h>

// Quarter turns (0-3) applied to the whole grid, LEDs and buttons alike,
// so that row 0 is the edge where the USB connector is.
#define BHOREAL_ROTATION 1

namespace bhoreal {

typedef void (*PressHandler)(uint8_t row, uint8_t col);

void begin();
void setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
void clear();
void show();
void scanButtons(PressHandler onPress);
bool isDown(uint8_t row, uint8_t col);  // button currently held

// Whether the helper ATmega328 answered during begin(), for diagnostics.
bool helperAnswered();
bool helperPowerAcked();

}  // namespace bhoreal
