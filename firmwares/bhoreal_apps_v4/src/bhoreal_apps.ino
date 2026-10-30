// Bhoreal SLIM apps: a start menu that launches one of several apps.
//
// Menu (each app is a 2x2 icon):
//   rows 3-4, cols 0-1  step sequencer (teal)
//   rows 3-4, cols 3-4  whack-a-mole (brown)
//   rows 3-4, cols 6-7  Simon (green, red, yellow, blue)
//   rows 6-7, cols 0-1  bouncing balls sequencer (two balls on a dark field)
//   rows 6-7, cols 3-4  memory / pairs (red and blue, crossed)
//   rows 6-7, cols 6-7  Frogger (a red car above, the green frog below)
//
// From any app, hold the two top corners together for 1.5 s to come back
// to the menu.

#include <Arduino.h>
#include "Apps.h"
#include "BhorealHw.h"
#include "UsbMidi.h"

const uint16_t HOME_HOLD_MS = 1500;

struct Entry {
  const App* app;
  uint8_t row, col;  // top-left corner of the 2x2 icon
  const char* name;
};

const Entry entries[] = {
  {&seq::app, 3, 0, "seq"},
  {&talps::app, 3, 3, "talps"},
  {&simon::app, 3, 6, "simon"},
  {&balls::app, 6, 0, "balls"},
  {&memory::app, 6, 3, "memory"},
  {&frogger::app, 6, 6, "frogger"},
};
const uint8_t ENTRY_COUNT = sizeof(entries) / sizeof(entries[0]);

int8_t running = -1;  // index into entries, -1 = menu
bool menuDirty = true;
bool waitForRelease = false;  // after leaving an app, until every button is up

void drawMenu() {
  bhoreal::clear();
  // Sequencer: teal block.
  for (uint8_t r = 0; r < 2; r++)
    for (uint8_t c = 0; c < 2; c++) bhoreal::setPixel(3 + r, 0 + c, 0, 90, 50);
  // Whack-a-mole: brown block.
  for (uint8_t r = 0; r < 2; r++)
    for (uint8_t c = 0; c < 2; c++) bhoreal::setPixel(3 + r, 3 + c, 90, 35, 0);
  // Simon: its four colours.
  bhoreal::setPixel(3, 6, 0, 120, 0);
  bhoreal::setPixel(3, 7, 120, 0, 0);
  bhoreal::setPixel(4, 6, 110, 90, 0);
  bhoreal::setPixel(4, 7, 0, 30, 130);
  // Bouncing balls: a cyan and a magenta ball on a dark blue field.
  bhoreal::setPixel(6, 0, 0, 140, 160);
  bhoreal::setPixel(6, 1, 0, 0, 40);
  bhoreal::setPixel(7, 0, 0, 0, 40);
  bhoreal::setPixel(7, 1, 160, 0, 140);
  // Memory: two pairs, red and blue, crossed.
  bhoreal::setPixel(6, 3, 130, 0, 0);
  bhoreal::setPixel(6, 4, 0, 0, 140);
  bhoreal::setPixel(7, 3, 0, 0, 140);
  bhoreal::setPixel(7, 4, 130, 0, 0);
  // Frogger: a red car on grey road, the green frog on the pavement.
  bhoreal::setPixel(6, 6, 30, 30, 30);
  bhoreal::setPixel(6, 7, 200, 0, 0);
  bhoreal::setPixel(7, 6, 40, 220, 0);
  bhoreal::setPixel(7, 7, 30, 30, 30);
  bhoreal::show();
}

void launch(uint8_t i) {
  running = i;
  entries[i].app->enter();
}

void goHome() {
  if (running >= 0) entries[running].app->exit();
  running = -1;
  menuDirty = true;
  waitForRelease = true;  // the corners are still held: don't let them pick an app
}

void onPress(uint8_t row, uint8_t col) {
  if (running >= 0) {
    entries[running].app->press(row, col);
    return;
  }
  if (waitForRelease) return;
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    const Entry& e = entries[i];
    if (row >= e.row && row <= e.row + 1 && col >= e.col && col <= e.col + 1) {
      launch(i);
      return;
    }
  }
}

// Both top corners held for a while = back to the menu.
void checkHomeCombo() {
  static uint32_t heldSince = 0;
  bool held = bhoreal::isDown(0, 0) && bhoreal::isDown(0, 7);
  if (!held || running < 0) {
    heldSince = 0;
    return;
  }
  if (heldSince == 0) heldSince = millis() | 1;
  else if (millis() - heldSince >= HOME_HOLD_MS) {
    heldSince = 0;
    goHome();
  }
}

void startupAnimation() {
  for (uint8_t row = 0; row < 8; row++) {
    bhoreal::clear();
    for (uint8_t col = 0; col < 8; col++) {
      uint8_t phase = col * 32;
      uint8_t r = phase < 128 ? 60 - phase / 3 : 0;
      uint8_t g = phase < 128 ? phase / 3 : 60 - (phase - 128) / 3;
      uint8_t b = phase < 128 ? 0 : (phase - 128) / 3;
      bhoreal::setPixel(row, col, r, g, b);
    }
    bhoreal::show();
    delay(80);
  }
}

void reportStatus() {
  static uint32_t lastReport;
  if (millis() - lastReport < 2000) return;
  lastReport = millis();
  Serial.print(F("bhoreal-apps running="));
  Serial.println(running >= 0 ? entries[running].name : "menu");
}

void setup() {
  Serial.begin(115200);
  bhoreal::begin();
  startupAnimation();
}

void loop() {
  bhoreal::scanButtons(onPress);
  checkHomeCombo();
  if (waitForRelease && !bhoreal::anyDown()) waitForRelease = false;

  if (running >= 0) {
    entries[running].app->loop();
  } else if (menuDirty) {
    drawMenu();
    menuDirty = false;
  }

  midi.drain();
  if (Serial) reportStatus();
}
