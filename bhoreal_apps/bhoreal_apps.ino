// Bhoreal SLIM apps: a start menu that launches one of several apps.
// The same source builds two firmwares (see build.sh):
//
// Games (BHOREAL_GAMES), each app a 2x2 icon:
//   rows 0-1, cols 0-1  reaction duel for two (red half above, blue below)
//   rows 0-1, cols 3-4  Pong for two (red and blue paddles, white ball)
//   rows 3-4, cols 0-1  whack-a-mole (brown)
//   rows 3-4, cols 3-4  Simon (green, red, yellow, blue)
//   rows 6-7, cols 0-1  memory / pairs (red and blue, crossed)
//   rows 6-7, cols 3-4  Frogger (a red car above, the green frog below)
//   rows 3-4, cols 6-7  Snake (green snake, red apple)
//   rows 6-7, cols 6-7  Connect 4 (red and yellow discs on a blue board)
//
// Music (BHOREAL_MUSIC):
//   rows 3-4, cols 0-1  step sequencer (teal)
//   rows 6-7, cols 0-1  bouncing balls sequencer (two balls on a dark field)
//
// From any app, hold the two top corners together for 1.5 s to come back
// to the menu.

#include <Arduino.h>
#include "Apps.h"
#include "BhorealHw.h"
#include "UsbMidi.h"

const uint16_t HOME_HOLD_MS = 1500;

#if !defined(BHOREAL_GAMES) && !defined(BHOREAL_MUSIC)
#error "Build with -DBHOREAL_GAMES or -DBHOREAL_MUSIC (see build.sh)"
#endif

struct Entry {
  const App* app;
  uint8_t row, col;     // top-left corner of the 2x2 icon
  uint8_t icon[4][3];   // icon colours: top left, top right, bottom left, bottom right
  const char* name;
};

#define BROWN {90, 35, 0}
#define TEAL {0, 90, 50}
#define DARK_BLUE {0, 0, 40}
#define GREY {30, 30, 30}
#define RED {200, 0, 0}
#define BLUE {0, 60, 200}

const Entry entries[] = {
#ifdef BHOREAL_GAMES
  {&talps::app, 3, 0, {BROWN, BROWN, BROWN, BROWN}, "talps"},
  {&simon::app, 3, 3, {{0, 120, 0}, {120, 0, 0}, {110, 90, 0}, {0, 30, 130}}, "simon"},
  {&memory::app, 6, 0, {{130, 0, 0}, {0, 0, 140}, {0, 0, 140}, {130, 0, 0}}, "memory"},
  {&frogger::app, 6, 3, {GREY, {200, 0, 0}, {40, 220, 0}, GREY}, "frogger"},
  {&snake::app, 3, 6, {{60, 200, 0}, {200, 0, 0}, {0, 60, 0}, {0, 0, 0}}, "snake"},
  {&connect4::app, 6, 6, {DARK_BLUE, {190, 150, 0}, {200, 0, 0}, {190, 150, 0}}, "connect4"},
  {&duel::app, 0, 0, {RED, RED, BLUE, BLUE}, "duel"},
  {&pong::app, 0, 3, {RED, {150, 150, 150}, {0, 0, 0}, BLUE}, "pong"},
#endif
#ifdef BHOREAL_MUSIC
  {&seq::app, 3, 0, {TEAL, TEAL, TEAL, TEAL}, "seq"},
  {&balls::app, 6, 0, {{0, 140, 160}, DARK_BLUE, DARK_BLUE, {160, 0, 140}}, "balls"},
#endif
};
const uint8_t ENTRY_COUNT = sizeof(entries) / sizeof(entries[0]);

int8_t running = -1;  // index into entries, -1 = menu
bool menuDirty = true;
bool waitForRelease = false;  // after leaving an app, until every button is up

void drawMenu() {
  bhoreal::clear();
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    const Entry& e = entries[i];
    for (uint8_t k = 0; k < 4; k++)
      bhoreal::setPixel(e.row + k / 2, e.col + k % 2, e.icon[k][0], e.icon[k][1], e.icon[k][2]);
  }
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
