// Frogger: get the frog across the road into the four homes at the top.
//
// Grid layout:
//   row 0      idle: col 0-4 = starting level 1-5 (green to red bar);
//              playing: col 0-4 = lives left. col 6 = start, col 7 = stop
//   row 1      homes at cols 0, 2, 5, 7; the cells between are hedge
//   rows 2-6   five lanes of traffic, each with its own direction and
//              speed. On level 5 the top two lanes become a river: ride
//              the logs, the water is deadly.
//   row 7      pavement, where the frog starts
//
// Press the cell next to the frog (up, down, left or right) to jump there.
// A car hitting the frog costs a life. Fill all four homes to go up a
// level; traffic gets faster. At game over the number of homes reached is
// shown in digits (gold if all five levels were cleared).

#include "Apps.h"
#include "BhorealHw.h"

namespace frogger {

const uint8_t CONTROL_ROW = 0;
const uint8_t LEVELS = 5;
const uint8_t START_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t MAX_LIVES = 3;
const uint8_t HOME_ROW = 1;
const uint8_t FIRST_LANE_ROW = 2;
const uint8_t LANES = 5;
const uint8_t START_ROW = 7;
const uint8_t START_FROG_COL = 3;
const uint8_t HOMES = 4;
const uint8_t homeCols[HOMES] = {0, 2, 5, 7};
const uint8_t LOOP = 16;  // each lane is a loop of 16 cells, 8 of them visible

const uint16_t DEATH_MS = 800;
const uint16_t LEVEL_UP_MS = 1200;

// Lanes top to bottom, bit i = cell i of the loop holds a car (or a log on
// the river). Kept as numbers rather than strings to save RAM.
const uint16_t roadPattern[LANES] = {
  0x1063,  // XX...XX.....X...
  0x0E07,  // XXX......XXX....
  0x4411,  // X...X.....X...X.
  0x3183,  // XX.....XX...XX..
  0x0407,  // XXX.......X.....
};
const uint16_t riverPattern[2] = {
  0x070F,  // XXXX....XXX.....
  0x03C7,  // XXX...XXXX......
};
const int8_t laneDir[LANES] = {-1, 1, -1, 1, -1};
const uint16_t lanePeriod[LANES] = {700, 900, 450, 600, 800};  // ms per shift, level 1
const uint8_t levelPct[LEVELS] = {100, 85, 72, 60, 52};
const uint8_t carColor[LANES][3] = {
  {200, 0, 0}, {160, 160, 160}, {180, 0, 160}, {200, 90, 0}, {0, 120, 200},
};
const uint8_t FROG[3] = {40, 220, 0};
const uint8_t WATER[3] = {0, 0, 45};
const uint8_t LOG[3] = {110, 45, 0};

enum State : uint8_t { IDLE, PLAYING, DYING, LEVEL_UP, GAME_OVER };

State state = IDLE;
uint8_t startLevel = 1;
uint8_t level;
uint8_t lives;
uint8_t homesFilled;  // bit h = home h reached on this level
uint16_t score;       // homes reached in the whole game
bool clearedAll;
uint8_t frogRow, frogCol;
uint16_t lane[LANES];  // bit i = cell i of the loop is occupied
uint8_t offset[LANES];
uint32_t nextShift[LANES];
uint32_t stateUntil;
bool dirty = true;

bool isRiver(uint8_t l) { return level == LEVELS && l < 2; }

bool occupied(uint8_t l, uint8_t col) {
  return lane[l] & (1u << ((col + offset[l]) % LOOP));
}

uint16_t periodFor(uint8_t l) {
  return (uint32_t)lanePeriod[l] * levelPct[level - 1] / 100;
}

void setupLevel() {
  uint32_t now = millis();
  for (uint8_t l = 0; l < LANES; l++) {
    lane[l] = isRiver(l) ? riverPattern[l] : roadPattern[l];
    offset[l] = random(LOOP);
    nextShift[l] = now + periodFor(l);
  }
  homesFilled = 0;
  frogRow = START_ROW;
  frogCol = START_FROG_COL;
}

void resetFrog() {
  frogRow = START_ROW;
  frogCol = START_FROG_COL;
}

void die() {
  state = DYING;
  stateUntil = millis() + DEATH_MS;
  dirty = true;
}

// True if the frog is somewhere deadly right now.
bool frogInDanger() {
  if (frogRow < FIRST_LANE_ROW || frogRow >= FIRST_LANE_ROW + LANES) return false;
  uint8_t l = frogRow - FIRST_LANE_ROW;
  bool hit = occupied(l, frogCol);
  return isRiver(l) ? !hit : hit;  // on the river you must be on a log
}

void startGame() {
  randomSeed(micros() ^ analogRead(A1));
  level = startLevel;
  lives = MAX_LIVES;
  score = 0;
  clearedAll = false;
  setupLevel();
  state = PLAYING;
}

void tryMove(uint8_t row, uint8_t col) {
  int8_t dr = (int8_t)row - frogRow, dc = (int8_t)col - frogCol;
  if (abs(dr) + abs(dc) != 1) return;  // only the four neighbours
  if (row == CONTROL_ROW) return;

  if (row == HOME_ROW) {
    for (uint8_t h = 0; h < HOMES; h++) {
      if (homeCols[h] != col) continue;
      if (homesFilled & (1 << h)) return;  // already taken
      homesFilled |= 1 << h;
      score++;
      if (homesFilled == (1 << HOMES) - 1) {
        state = LEVEL_UP;
        stateUntil = millis() + LEVEL_UP_MS;
      } else {
        resetFrog();
      }
      return;
    }
    return;  // hedge: can't jump there
  }

  frogRow = row;
  frogCol = col;
  if (frogInDanger()) die();
}

void press(uint8_t row, uint8_t col) {
  dirty = true;
  if (row == CONTROL_ROW) {
    bool busy = state == PLAYING || state == DYING || state == LEVEL_UP;
    if (col < LEVELS && !busy) startLevel = col + 1;
    else if (col == START_COL && !busy) startGame();
    else if (col == STOP_COL) state = IDLE;
    return;
  }
  if (state == PLAYING) tryMove(row, col);
}

void update() {
  uint32_t now = millis();

  if (state == DYING || state == LEVEL_UP) {
    if ((int32_t)(now - stateUntil) < 0) return;
    dirty = true;
    if (state == DYING) {
      if (--lives == 0) {
        state = GAME_OVER;
        return;
      }
      resetFrog();
      state = PLAYING;
    } else if (level == LEVELS) {
      clearedAll = true;
      state = GAME_OVER;
    } else {
      level++;
      setupLevel();
      state = PLAYING;
    }
    return;
  }
  if (state != PLAYING) return;

  for (uint8_t l = 0; l < LANES; l++) {
    if ((int32_t)(now - nextShift[l]) < 0) continue;
    nextShift[l] += periodFor(l);
    // Content moving left means reading further along the loop.
    offset[l] = (offset[l] + (laneDir[l] < 0 ? 1 : LOOP - 1)) % LOOP;
    dirty = true;

    // A frog riding a log moves with it, and falls in if carried off-screen.
    if (isRiver(l) && frogRow == FIRST_LANE_ROW + l) {
      int8_t col = frogCol + laneDir[l];
      if (col < 0 || col > 7) { die(); return; }
      frogCol = col;
    }
  }
  if (frogInDanger()) die();
}

void render() {
  bhoreal::clear();

  // Controls. Nothing static goes below ~30: these LEDs flicker below that.
  bool busy = state == PLAYING || state == DYING || state == LEVEL_UP;
  if (busy) {
    for (uint8_t i = 0; i < lives; i++) bhoreal::setPixel(CONTROL_ROW, i, 140, 0, 0);
  } else {
    static const uint8_t levelColor[LEVELS][3] = {
      {0, 120, 0}, {70, 120, 0}, {120, 110, 0}, {140, 50, 0}, {150, 0, 0},
    };
    for (uint8_t i = 0; i < startLevel; i++)
      bhoreal::setPixel(CONTROL_ROW, i, levelColor[i][0], levelColor[i][1], levelColor[i][2]);
  }
  bhoreal::setPixel(CONTROL_ROW, START_COL, 0, busy ? 30 : 80, 0);
  bhoreal::setPixel(CONTROL_ROW, STOP_COL, busy ? 80 : 30, 0, 0);

  if (state == GAME_OVER) {
    if (clearedAll) drawNumber(1, score, 160, 120, 0);
    else drawNumber(1, score, 70, 60, 0);
    bhoreal::show();
    return;
  }

  // Homes: empty = dim green, reached = a bright frog. Level-up flashes them.
  bool flashOff = state == LEVEL_UP && ((millis() / 200) % 2);
  for (uint8_t h = 0; h < HOMES; h++) {
    if (flashOff) continue;
    if (state != IDLE && (homesFilled & (1 << h)))
      bhoreal::setPixel(HOME_ROW, homeCols[h], FROG[0], FROG[1], FROG[2]);
    else
      bhoreal::setPixel(HOME_ROW, homeCols[h], 0, 45, 0);
  }

  if (state != IDLE) {
    for (uint8_t l = 0; l < LANES; l++) {
      uint8_t row = FIRST_LANE_ROW + l;
      for (uint8_t c = 0; c < 8; c++) {
        bool on = occupied(l, c);
        if (isRiver(l)) {
          const uint8_t* color = on ? LOG : WATER;
          bhoreal::setPixel(row, c, color[0], color[1], color[2]);
        } else if (on) {
          bhoreal::setPixel(row, c, carColor[l][0], carColor[l][1], carColor[l][2]);
        }
      }
    }
    if (state == DYING) bhoreal::setPixel(frogRow, frogCol, 220, 0, 0);
    else if (state == PLAYING) bhoreal::setPixel(frogRow, frogCol, FROG[0], FROG[1], FROG[2]);
  } else {
    bhoreal::setPixel(START_ROW, START_FROG_COL, FROG[0], FROG[1], FROG[2]);
  }
  bhoreal::show();
}

void enter() {
  state = IDLE;
  dirty = true;
}

void loop() {
  update();
  if (state == LEVEL_UP) dirty = true;  // homes flash
  if (dirty) {
    render();
    dirty = false;
  }
}

void exit() {
  state = IDLE;
}

}  // namespace frogger

const App frogger::app = {frogger::enter, frogger::loop, frogger::press, frogger::exit};
