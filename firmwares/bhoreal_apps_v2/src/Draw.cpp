#include "Apps.h"
#include "BhorealHw.h"

// 3x5 digits, one byte per row, bit 2 = left column.
static const uint8_t digits[10][5] PROGMEM = {
  {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
  {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

void drawNumber(uint8_t firstRow, uint16_t value, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t shown = min(value, (uint16_t)99);
  uint8_t tens = shown / 10, ones = shown % 10;
  for (uint8_t y = 0; y < 5; y++) {
    uint8_t t = pgm_read_byte(&digits[tens][y]);
    uint8_t o = pgm_read_byte(&digits[ones][y]);
    for (uint8_t x = 0; x < 3; x++) {
      if (tens && (t & (4 >> x))) bhoreal::setPixel(firstRow + y, 1 + x, r, g, b);
      if (o & (4 >> x)) bhoreal::setPixel(firstRow + y, 5 + x, r, g, b);
    }
  }
}
