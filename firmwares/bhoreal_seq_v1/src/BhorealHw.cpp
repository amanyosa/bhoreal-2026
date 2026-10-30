#include "BhorealHw.h"
#include <Wire.h>

namespace bhoreal {

static const uint8_t NUM_LEDS = 64;
static const uint8_t SLAVE_ADDR = 4;     // helper ATmega328 on the I2C bus
static const uint8_t DEBOUNCE_MS = 25;

// LED chain index for each grid position (remapSlim[0] in the original firmware).
static const uint8_t ledIndex[8][8] PROGMEM = {
  {7, 8, 23, 24, 39, 40, 55, 56},
  {6, 9, 22, 25, 38, 41, 54, 57},
  {5, 10, 21, 26, 37, 42, 53, 58},
  {4, 11, 20, 27, 36, 43, 52, 59},
  {3, 12, 19, 28, 35, 44, 51, 60},
  {2, 13, 18, 29, 34, 45, 50, 61},
  {1, 14, 17, 30, 33, 46, 49, 62},
  {0, 15, 16, 31, 32, 47, 48, 63},
};

// Order in which the 74HC165 shifts out the rows.
static const uint8_t rowOrder[8] = {4, 5, 6, 7, 0, 1, 2, 3};

static uint8_t pixels[NUM_LEDS * 3];     // GRB, as the WS2812 expects
static uint8_t shown[NUM_LEDS * 3];      // last frame sent to the LEDs
static bool everShown = false;
static bool pressed[8][8];
static uint8_t lastChange[8][8];         // low byte of millis()
static uint32_t lastScanUs;

// Logical (row, col) -> physical (row, col), turning clockwise.
static void toPhysical(uint8_t& r, uint8_t& c) {
  uint8_t lr = r, lc = c;
  switch (BHOREAL_ROTATION & 3) {
    case 1: r = lc;     c = 7 - lr; break;
    case 2: r = 7 - lr; c = 7 - lc; break;
    case 3: r = 7 - lc; c = lr;     break;
  }
}

// Physical (row, col) -> logical (row, col), the inverse of toPhysical().
static void toLogical(uint8_t& r, uint8_t& c) {
  uint8_t pr = r, pc = c;
  switch (BHOREAL_ROTATION & 3) {
    case 1: r = 7 - pc; c = pr;     break;
    case 2: r = 7 - pr; c = 7 - pc; break;
    case 3: r = pc;     c = 7 - pr; break;
  }
}

static bool slaveAnswered;
static bool powerAcked;

static bool slaveSend(uint8_t val) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool helperAnswered() { return slaveAnswered; }
bool helperPowerAcked() { return powerAcked; }

void begin() {
  DDRB |= B11110000;   // 11,10,9,8 OUTPUT
  DDRC |= B01000000;   // 5  OUTPUT
  DDRC &= B01111111;   // 13 INPUT
  DDRE &= B10111111;   // 7 INPUT
  DDRD |= B11010000;   // 12, 6, 4 OUTPUT
  DDRF |= B00000011;   // A4, A5 OUTPUT
  PORTF |= B00000010;  // A4 HIGH
  PORTF &= B11111110;  // A5 LOW
  PORTD |= B01010000;  // 12, 4 HIGH
  PORTD &= B01111111;  // 6 LOW (LED data idle)
  PORTB &= B01111111;  // 11 LOW, reset helper ATmega328
  PORTB |= B10000000;  // 11 HIGH

  // Wake the helper ATmega328, as the original firmware does. It has just
  // been reset and needs time to boot, so wait up to 5 s like the original.
  Wire.begin();
  Wire.setWireTimeout(3000, true);
  uint32_t start = millis();
  do {
    slaveSend(3);
    Wire.requestFrom(SLAVE_ADDR, (uint8_t)1);
    if (Wire.available()) { slaveAnswered = true; break; }
    delay(4);
  } while (millis() - start < 5000);
  while (Wire.available()) Wire.read();

  PORTD &= B11101111;  // POWER_VCC (pin 4) LOW: power on
  for (uint8_t i = 0; i < 50 && !powerAcked; i++) {
    powerAcked = slaveSend(1);
    if (!powerAcked) delay(10);
  }

  for (uint8_t r = 0; r < 8; r++)
    for (uint8_t c = 0; c < 8; c++) pressed[r][c] = false;
  clear();
  show();
}

void setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
  toPhysical(row, col);
  uint8_t n = pgm_read_byte(&ledIndex[row][col]);
  uint8_t* p = &pixels[n * 3];
  p[0] = g;
  p[1] = r;
  p[2] = b;
}

void clear() {
  memset(pixels, 0, sizeof(pixels));
}

// WS2812 bit-bang for a 16 MHz AVR, taken from the original firmware
// (itself based on Adafruit NeoPixel). Pin 6 = PD7.
void show() {
  // Re-sending an identical frame can only add glitches; skip it.
  if (everShown && memcmp(pixels, shown, sizeof(pixels)) == 0) return;
  memcpy(shown, pixels, sizeof(pixels));
  everShown = true;

  volatile uint8_t* port = &PORTD;
  const uint8_t pinMask = _BV(7);

  noInterrupts();
  volatile uint16_t i = sizeof(pixels);
  volatile uint8_t* ptr = pixels;
  volatile uint8_t b = *ptr++;
  volatile uint8_t hi = *port | pinMask;
  volatile uint8_t lo = *port & ~pinMask;
  volatile uint8_t next = lo;
  volatile uint8_t bit = 8;

  asm volatile(
   "head20:"                   "\n\t"
    "st   %a[port],  %[hi]"    "\n\t"
    "sbrc %[byte],  7"         "\n\t"
     "mov  %[next], %[hi]"     "\n\t"
    "dec  %[bit]"              "\n\t"
    "st   %a[port],  %[next]"  "\n\t"
    "mov  %[next] ,  %[lo]"    "\n\t"
    "breq nextbyte20"          "\n\t"
    "rol  %[byte]"             "\n\t"
    "rjmp .+0"                 "\n\t"
    "nop"                      "\n\t"
    "st   %a[port],  %[lo]"    "\n\t"
    "nop"                      "\n\t"
    "rjmp .+0"                 "\n\t"
    "rjmp head20"              "\n\t"
   "nextbyte20:"               "\n\t"
    "ldi  %[bit]  ,  8"        "\n\t"
    "ld   %[byte] ,  %a[ptr]+" "\n\t"
    "st   %a[port], %[lo]"     "\n\t"
    "nop"                      "\n\t"
    "sbiw %[count], 1"         "\n\t"
     "brne head20"             "\n"
    : [port]  "+e" (port),
      [byte]  "+r" (b),
      [bit]   "+r" (bit),
      [next]  "+r" (next),
      [count] "+w" (i)
    : [ptr]    "e" (ptr),
      [hi]     "r" (hi),
      [lo]     "r" (lo));

  interrupts();
  delayMicroseconds(60);  // latch
}

bool isDown(uint8_t row, uint8_t col) {
  toPhysical(row, col);
  return pressed[row][col];
}

// Same shift-register sequence as checkMatrix() in the original firmware.
// A button reads 0 while pressed.
void scanButtons(PressHandler onPress) {
  uint32_t now = micros();
  if (now - lastScanUs < 1000) return;
  lastScanUs = now;
  uint8_t ms = millis();

  PORTB &= B11101111;  // CLOCKPIN (8) LOW
  PORTB |= B00100000;  // DATAPIN (9) HIGH
  for (uint8_t c = 0; c < 8; c++) {
    PORTB |= B00010000;  // CLOCKPIN HIGH
    PORTB &= B10111111;  // INLOADPIN (10) LOW: latch inputs
    delayMicroseconds(1);
    PORTB |= B01000000;  // INLOADPIN HIGH

    for (uint8_t i = 0; i < 8; i++) {
      PORTC &= B10111111;  // INCLOCKPIN (5) LOW
      uint8_t r = rowOrder[i];
      bool down = !(PINC >> 7);  // INDATAPIN (13)
      if (down != pressed[r][c] && uint8_t(ms - lastChange[r][c]) >= DEBOUNCE_MS) {
        pressed[r][c] = down;
        lastChange[r][c] = ms;
        if (down) {
          uint8_t lr = r, lc = c;
          toLogical(lr, lc);
          onPress(lr, lc);
        }
      }
      PORTC |= B01000000;  // INCLOCKPIN HIGH
    }

    PORTB &= B11101111;  // CLOCKPIN LOW
    PORTB &= B11011111;  // DATAPIN LOW
  }
}

}  // namespace bhoreal
