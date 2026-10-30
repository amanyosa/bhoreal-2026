// Bhoreal SLIM apps: a start menu that launches one of several apps.
//
// Menu (rows 3-4, each app is a 2x2 icon):
//   cols 0-1  step sequencer (teal)
//   cols 3-4  whack-a-mole (brown)
//   cols 6-7  Simon (green, red, yellow, blue)
//
// From any app, hold the two top corners together for 1.5 s to come back
// to the menu.

#include <Arduino.h>
#include "Apps.h"
#include "BhorealHw.h"
#include "UsbMidi.h"

const uint8_t ICON_ROW = 3;
const uint16_t HOME_HOLD_MS = 1500;

struct Entry {
  const App* app;
  uint8_t col;  // left column of the 2x2 icon
  const char* name;
};

const Entry entries[] = {
  {&seq::app, 0, "seq"},
  {&talps::app, 3, "talps"},
  {&simon::app, 6, "simon"},
};
const uint8_t ENTRY_COUNT = sizeof(entries) / sizeof(entries[0]);

int8_t running = -1;  // index into entries, -1 = menu
bool menuDirty = true;

void drawMenu() {
  bhoreal::clear();
  // Sequencer: teal block.
  for (uint8_t r = 0; r < 2; r++)
    for (uint8_t c = 0; c < 2; c++) bhoreal::setPixel(ICON_ROW + r, 0 + c, 0, 90, 50);
  // Whack-a-mole: brown block.
  for (uint8_t r = 0; r < 2; r++)
    for (uint8_t c = 0; c < 2; c++) bhoreal::setPixel(ICON_ROW + r, 3 + c, 90, 35, 0);
  // Simon: its four colours.
  bhoreal::setPixel(ICON_ROW, 6, 0, 120, 0);
  bhoreal::setPixel(ICON_ROW, 7, 120, 0, 0);
  bhoreal::setPixel(ICON_ROW + 1, 6, 110, 90, 0);
  bhoreal::setPixel(ICON_ROW + 1, 7, 0, 30, 130);
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
}

void onPress(uint8_t row, uint8_t col) {
  if (running >= 0) {
    entries[running].app->press(row, col);
    return;
  }
  if (row != ICON_ROW && row != ICON_ROW + 1) return;
  for (uint8_t i = 0; i < ENTRY_COUNT; i++)
    if (col == entries[i].col || col == entries[i].col + 1) launch(i);
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

  if (running >= 0) {
    entries[running].app->loop();
  } else if (menuDirty) {
    drawMenu();
    menuDirty = false;
  }

  midi.drain();
  if (Serial) reportStatus();
}
