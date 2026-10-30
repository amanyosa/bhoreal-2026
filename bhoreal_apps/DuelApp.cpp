// Reaction duel for two players, one at each end of the grid: the top
// player owns rows 0-3 (red), the bottom player rows 4-7 (blue). Wait for
// the middle to light up white, then tap anywhere in your half before the
// other player does. Tapping before the light is a false start and the
// point goes to the other player. First to 5 points wins.
//
// Grid layout:
//   row 0      top player's points, counted from their left (cols 7..3)
//   rows 3-4   the signal: white when it is time to tap
//   row 7      bottom player's points (cols 0..4)
// The winner of each point sees their half light up in their colour.
// Tap anywhere to start a match.

#include "Apps.h"
#include "BhorealHw.h"

namespace duel {

const uint8_t WIN_POINTS = 5;
const uint16_t MIN_WAIT_MS = 1500;
const uint16_t MAX_WAIT_MS = 4500;
const uint16_t POINT_MS = 1500;
const uint16_t MATCH_OVER_MS = 4000;

enum Player : uint8_t { TOP, BOTTOM };
const uint8_t color[2][3] = {{200, 0, 0}, {0, 60, 200}};

enum State : uint8_t { IDLE, WAITING, SIGNAL, POINT, MATCH_OVER };

State state = IDLE;
uint8_t score[2];
Player lastWinner;
uint32_t until;
bool dirty = true;

void startWait() {
  state = WAITING;
  until = millis() + random(MIN_WAIT_MS, MAX_WAIT_MS);
  dirty = true;
}

void startMatch() {
  randomSeed(micros() ^ analogRead(A1));
  score[TOP] = score[BOTTOM] = 0;
  startWait();
}

void award(Player p) {
  score[p]++;
  lastWinner = p;
  state = score[p] >= WIN_POINTS ? MATCH_OVER : POINT;
  until = millis() + (state == MATCH_OVER ? MATCH_OVER_MS : POINT_MS);
  dirty = true;
}

void press(uint8_t row, uint8_t col) {
  Player p = row < 4 ? TOP : BOTTOM;
  switch (state) {
    case IDLE: startMatch(); break;
    case WAITING: award(p == TOP ? BOTTOM : TOP); break;  // false start
    case SIGNAL: award(p); break;
    default: break;
  }
}

void update() {
  if (state == IDLE) return;
  if (state == MATCH_OVER) dirty = true;  // flashing
  if ((int32_t)(millis() - until) < 0) return;
  switch (state) {
    case WAITING:
      state = SIGNAL;
      dirty = true;
      break;
    case POINT: startWait(); break;
    case MATCH_OVER:
      state = IDLE;
      dirty = true;
      break;
    default: break;
  }
}

// Each player's points, read from their own side of the grid.
void drawScores() {
  for (uint8_t i = 0; i < score[TOP]; i++)
    bhoreal::setPixel(0, 7 - i, color[TOP][0], color[TOP][1], color[TOP][2]);
  for (uint8_t i = 0; i < score[BOTTOM]; i++)
    bhoreal::setPixel(7, i, color[BOTTOM][0], color[BOTTOM][1], color[BOTTOM][2]);
}

void fillRows(uint8_t first, uint8_t last, const uint8_t* c) {
  for (uint8_t r = first; r <= last; r++)
    for (uint8_t k = 0; k < 8; k++) bhoreal::setPixel(r, k, c[0], c[1], c[2]);
}

void render() {
  bhoreal::clear();
  // Nothing static goes below ~30: these LEDs flicker below that.
  const uint8_t* w = color[lastWinner];
  const uint8_t half[3] = {(uint8_t)(w[0] / 2), (uint8_t)(w[1] / 2), (uint8_t)(w[2] / 2)};
  switch (state) {
    case IDLE:
      // One bar of each colour: who sits where.
      fillRows(1, 1, color[TOP]);
      fillRows(6, 6, color[BOTTOM]);
      break;
    case SIGNAL: {
      static const uint8_t white[3] = {150, 150, 150};
      fillRows(3, 4, white);
      break;
    }
    case POINT:
      if (lastWinner == TOP) fillRows(1, 3, half);
      else fillRows(4, 6, half);
      break;
    case MATCH_OVER:
      if ((millis() / 250) % 2 == 0) {
        if (lastWinner == TOP) fillRows(0, 3, w);
        else fillRows(4, 7, w);
      }
      break;
    default: break;
  }
  if (state != IDLE && state != MATCH_OVER) drawScores();
  bhoreal::show();
}

void enter() {
  state = IDLE;
  dirty = true;
}

void loop() {
  update();
  if (dirty) {
    render();
    dirty = false;
  }
}

void exit() {
  state = IDLE;
}

}  // namespace duel

const App duel::app = {duel::enter, duel::loop, duel::press, duel::exit};
