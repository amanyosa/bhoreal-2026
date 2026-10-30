// Snake: eat the red apples and grow, without biting your own tail.
//
// Grid layout:
//   row 0      col 0-2 = speed 1-3 (green to red bar), col 6 = start,
//              col 7 = stop
//   rows 1-7   the field. The snake wraps around the edges (kinder for
//              small children); only biting itself ends the game.
//
// To steer, tap the field on the side you want to turn to: while moving up
// or down, tap left or right of the head; while moving sideways, tap above
// or below it. At game over the number of apples eaten is shown in digits.

#include "Apps.h"
#include "BhorealHw.h"

namespace snake {

const uint8_t CONTROL_ROW = 0;
const uint8_t SPEEDS = 3;
const uint8_t START_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t FIRST_ROW = 1;
const uint8_t ROWS = 7;
const uint8_t COLS = 8;
const uint8_t CELLS = ROWS * COLS;
const uint8_t START_LENGTH = 3;
const uint16_t stepMs[SPEEDS] = {450, 320, 220};
const uint16_t CRASH_MS = 1200;

enum Dir : uint8_t { UP, RIGHT, DOWN, LEFT };
const int8_t dRow[4] = {-1, 0, 1, 0};
const int8_t dCol[4] = {0, 1, 0, -1};

enum State : uint8_t { IDLE, PLAYING, CRASHED, GAME_OVER };

State state = IDLE;
uint8_t speed = 1;
// Body as a ring buffer of field cells (row * COLS + col), head at `head`.
uint8_t body[CELLS];
uint8_t head, length;
Dir dir, nextDir;
uint8_t apple;
uint8_t eaten;
uint32_t nextStep;
uint32_t crashedAt;
bool dirty = true;

uint8_t cellAt(uint8_t i) { return body[(head + CELLS - i) % CELLS]; }  // i = 0 is the head

bool onSnake(uint8_t cell) {
  for (uint8_t i = 0; i < length; i++)
    if (cellAt(i) == cell) return true;
  return false;
}

void placeApple() {
  if (length >= CELLS) return;
  do { apple = random(CELLS); } while (onSnake(apple));
}

void startGame() {
  randomSeed(micros() ^ analogRead(A1));
  head = 0;
  length = START_LENGTH;
  // Start in the middle of the bottom half, heading up.
  for (uint8_t i = 0; i < START_LENGTH; i++) body[(CELLS - i) % CELLS] = (3 + i) * COLS + 3;
  dir = nextDir = UP;
  eaten = 0;
  placeApple();
  state = PLAYING;
  nextStep = millis() + stepMs[speed - 1];
}

void step() {
  dir = nextDir;
  uint8_t h = cellAt(0);
  int8_t row = h / COLS + dRow[dir];
  int8_t col = h % COLS + dCol[dir];
  row = (row + ROWS) % ROWS;  // wrap around the edges
  col = (col + COLS) % COLS;
  uint8_t next = row * COLS + col;

  bool grows = next == apple;
  // The tail moves away this step unless the snake grows, so it is safe.
  for (uint8_t i = 0; i < length - (grows ? 0 : 1); i++) {
    if (cellAt(i) == next) {
      state = CRASHED;
      crashedAt = millis();
      return;
    }
  }

  head = (head + 1) % CELLS;
  body[head] = next;
  if (grows) {
    length++;
    eaten++;
    placeApple();
  }
}

void press(uint8_t row, uint8_t col) {
  dirty = true;
  if (row == CONTROL_ROW) {
    bool busy = state == PLAYING || state == CRASHED;
    if (col < SPEEDS && !busy) speed = col + 1;
    else if (col == START_COL && !busy) startGame();
    else if (col == STOP_COL) state = IDLE;
    return;
  }
  if (state != PLAYING) return;

  uint8_t h = cellAt(0);
  int8_t dr = (int8_t)(row - FIRST_ROW) - h / COLS;
  int8_t dc = (int8_t)col - h % COLS;
  if (dir == UP || dir == DOWN) {
    if (dc > 0) nextDir = RIGHT;
    else if (dc < 0) nextDir = LEFT;
  } else {
    if (dr > 0) nextDir = DOWN;
    else if (dr < 0) nextDir = UP;
  }
}

void update() {
  uint32_t now = millis();
  if (state == CRASHED) {
    dirty = true;  // the body flashes red
    if (now - crashedAt >= CRASH_MS) state = GAME_OVER;
    return;
  }
  if (state != PLAYING || (int32_t)(now - nextStep) < 0) return;
  nextStep += stepMs[speed - 1];
  step();
  dirty = true;
}

void render() {
  bhoreal::clear();

  // Nothing static goes below ~30: these LEDs flicker below that.
  static const uint8_t speedColor[SPEEDS][3] = {{0, 120, 0}, {120, 110, 0}, {150, 0, 0}};
  bool busy = state == PLAYING || state == CRASHED;
  for (uint8_t i = 0; i < speed; i++)
    bhoreal::setPixel(CONTROL_ROW, i, speedColor[i][0], speedColor[i][1], speedColor[i][2]);
  bhoreal::setPixel(CONTROL_ROW, START_COL, 0, busy ? 30 : 80, 0);
  bhoreal::setPixel(CONTROL_ROW, STOP_COL, busy ? 80 : 30, 0, 0);

  if (state == GAME_OVER) {
    drawNumber(1, eaten, 70, 60, 0);
  } else if (state == IDLE) {
    // A little snake waiting, with an apple ahead.
    bhoreal::setPixel(5, 3, 0, 60, 0);
    bhoreal::setPixel(4, 3, 0, 60, 0);
    bhoreal::setPixel(3, 3, 60, 200, 0);
    bhoreal::setPixel(1, 3, 200, 0, 0);
  } else {
    bool flashOn = state == CRASHED && ((millis() - crashedAt) / 200) % 2 == 0;
    bhoreal::setPixel(FIRST_ROW + apple / COLS, apple % COLS, 200, 0, 0);
    for (uint8_t i = length; i-- > 0;) {
      uint8_t c = cellAt(i);
      uint8_t r = FIRST_ROW + c / COLS, k = c % COLS;
      if (state == CRASHED) bhoreal::setPixel(r, k, flashOn ? 200 : 40, 0, 0);
      else if (i == 0) bhoreal::setPixel(r, k, 60, 200, 0);
      else bhoreal::setPixel(r, k, 0, 60, 0);
    }
  }
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

}  // namespace snake

const App snake::app = {snake::enter, snake::loop, snake::press, snake::exit};
