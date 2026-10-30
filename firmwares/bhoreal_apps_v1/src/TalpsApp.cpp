// Whack-a-mole app ("pica el talp", was bhoreal_talps).
//
// Grid layout:
//   row 0      col 0-4 = choose level 1-5, col 6 = start, col 7 = stop
//   row 1      lives
//   row 2      margin (dark)
//   rows 3-7   holes; level n uses the first n of these rows
//
// Moles pop up and hide again; hit them before they hide. Each mole that
// escapes costs a life. Moles come faster as the score grows.
// Level 5 adds golden moles (3 points, quicker) and bombs (don't hit them).
// At game over the score is shown in digits on the field.

#include "Apps.h"
#include "BhorealHw.h"

namespace talps {

const uint8_t LEVEL_ROW = 0;
const uint8_t LEVELS = 5;
const uint8_t START_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t LIVES_ROW = 1;
const uint8_t MAX_LIVES = 5;
const uint8_t MARGIN_ROW = 2;
const uint8_t FIRST_FIELD_ROW = 3;
const uint8_t COLS = 8;

enum State : uint8_t { MENU, PLAYING, GAME_OVER };
enum MoleType : uint8_t { NORMAL, GOLDEN, BOMB };

struct Mole {
  bool active;
  uint8_t row, col;
  MoleType type;
  uint32_t hideAt;
};

struct LevelParams {
  uint16_t upMs;       // how long a mole stays up
  uint16_t spawnMs;    // time between new moles
  uint8_t maxMoles;    // moles up at the same time
};

const LevelParams levelParams[LEVELS] = {
  {1600, 1300, 1},
  {1400, 1100, 2},
  {1250, 950, 2},
  {1100, 800, 3},
  {1000, 700, 4},
};

const uint8_t MAX_MOLES = 4;
const uint16_t FLASH_MS = 180;

State state = MENU;
uint8_t level = 1;
uint8_t lives;
uint16_t score;
Mole moles[MAX_MOLES];
uint32_t nextSpawnAt;

// Short green/red flash on a hole after a hit or an escape.
struct Flash { uint8_t row, col; bool good; uint32_t until; };
Flash flashes[MAX_MOLES * 2];

bool dirty = true;

// Moles speed up by 4% every 5 points, down to 45% of the level's base time.
uint16_t scaled(uint16_t baseMs) {
  uint8_t pct = 100 - min(55, (int)(score / 5) * 4);
  return (uint32_t)baseMs * pct / 100;
}

void addFlash(uint8_t row, uint8_t col, bool good) {
  for (Flash& f : flashes) {
    if (f.until == 0 || (int32_t)(millis() - f.until) >= 0) {
      f = {row, col, good, millis() + FLASH_MS};
      return;
    }
  }
}

bool holeTaken(uint8_t row, uint8_t col) {
  for (const Mole& m : moles)
    if (m.active && m.row == row && m.col == col) return true;
  return false;
}

void spawnMole() {
  const LevelParams& p = levelParams[level - 1];
  uint8_t up = 0;
  for (const Mole& m : moles) up += m.active;
  if (up >= p.maxMoles) return;

  uint8_t row, col, tries = 0;
  do {
    row = FIRST_FIELD_ROW + random(level);
    col = random(COLS);
  } while (holeTaken(row, col) && ++tries < 20);
  if (holeTaken(row, col)) return;

  MoleType type = NORMAL;
  uint16_t upMs = scaled(p.upMs);
  if (level == LEVELS) {
    uint8_t roll = random(100);
    if (roll < 12) { type = GOLDEN; upMs = upMs * 6 / 10; }
    else if (roll < 27) type = BOMB;
  }

  for (Mole& m : moles) {
    if (!m.active) {
      m = {true, row, col, type, millis() + upMs};
      return;
    }
  }
}

void clearField() {
  for (Mole& m : moles) m.active = false;
  for (Flash& f : flashes) f.until = 0;
}

void loseLife() {
  if (lives > 0) lives--;
  if (lives == 0) {
    state = GAME_OVER;
    for (Mole& m : moles) m.active = false;
  }
}

void startGame() {
  randomSeed(micros() ^ analogRead(A1));
  state = PLAYING;
  lives = MAX_LIVES;
  score = 0;
  clearField();
  nextSpawnAt = millis() + 600;
}

void press(uint8_t row, uint8_t col) {
  if (row == LEVEL_ROW) {
    if (col < LEVELS && state != PLAYING) level = col + 1;
    else if (col == START_COL && state != PLAYING) startGame();
    else if (col == STOP_COL) {
      state = MENU;
      clearField();
    }
  } else if (state == PLAYING && row >= FIRST_FIELD_ROW) {
    for (Mole& m : moles) {
      if (!m.active || m.row != row || m.col != col) continue;
      m.active = false;
      if (m.type == BOMB) {
        addFlash(row, col, false);
        loseLife();
      } else {
        score += (m.type == GOLDEN) ? 3 : 1;
        addFlash(row, col, true);
      }
      break;
    }
  }
  dirty = true;
}

void update() {
  if (state != PLAYING) return;
  uint32_t now = millis();

  for (Mole& m : moles) {
    if (m.active && (int32_t)(now - m.hideAt) >= 0) {
      m.active = false;
      dirty = true;
      if (m.type != BOMB) {  // a bomb that hides on its own is harmless
        addFlash(m.row, m.col, false);
        loseLife();
        if (state != PLAYING) return;
      }
    }
  }

  if ((int32_t)(now - nextSpawnAt) >= 0) {
    spawnMole();
    nextSpawnAt = now + scaled(levelParams[level - 1].spawnMs);
    dirty = true;
  }
}

void render() {
  bhoreal::clear();

  // Level as a difficulty bar: buttons 1..level lit from green to red, the
  // rest off. Nothing static goes below ~30, since these LEDs flicker at very
  // low levels.
  static const uint8_t levelColor[LEVELS][3] = {
    {0, 120, 0}, {70, 120, 0}, {120, 110, 0}, {140, 50, 0}, {150, 0, 0},
  };
  for (uint8_t l = 0; l < level; l++)
    bhoreal::setPixel(LEVEL_ROW, l, levelColor[l][0], levelColor[l][1], levelColor[l][2]);
  bhoreal::setPixel(LEVEL_ROW, START_COL, 0, state == PLAYING ? 30 : 80, 0);
  bhoreal::setPixel(LEVEL_ROW, STOP_COL, state == PLAYING ? 80 : 30, 0, 0);

  if (state == PLAYING)
    for (uint8_t i = 0; i < lives; i++) bhoreal::setPixel(LIVES_ROW, i, 70, 0, 0);

  if (state == MENU) {
    // Preview the holes the chosen level will use.
    for (uint8_t r = 0; r < level; r++)
      for (uint8_t c = 0; c < COLS; c++) bhoreal::setPixel(FIRST_FIELD_ROW + r, c, 70, 30, 0);
  } else if (state == GAME_OVER) {
    drawNumber(FIRST_FIELD_ROW, score, 70, 60, 0);
    // Each full hundred lights one LED in the margin row.
    for (uint8_t h = 0; h < min(score / 100, 8); h++) bhoreal::setPixel(MARGIN_ROW, h, 70, 60, 0);
  } else {
    for (const Mole& m : moles) {
      if (!m.active) continue;
      if (m.type == GOLDEN) bhoreal::setPixel(m.row, m.col, 120, 90, 0);
      else if (m.type == BOMB) bhoreal::setPixel(m.row, m.col, 0, 30, 120);
      else bhoreal::setPixel(m.row, m.col, 90, 35, 0);
    }
  }

  uint32_t now = millis();
  for (const Flash& f : flashes) {
    if (f.until && (int32_t)(now - f.until) < 0) {
      if (f.good) bhoreal::setPixel(f.row, f.col, 0, 120, 0);
      else bhoreal::setPixel(f.row, f.col, 120, 0, 0);
    }
  }
  bhoreal::show();
}

void enter() {
  state = MENU;
  clearField();
  dirty = true;
}

void loop() {
  update();

  // Redraw while flashes are running so they switch off on time.
  static uint32_t lastFrame;
  if (millis() - lastFrame >= 30) {
    lastFrame = millis();
    for (const Flash& f : flashes)
      if (f.until && (int32_t)(millis() - f.until) < (int32_t)FLASH_MS) dirty = true;
  }

  if (dirty) {
    render();
    dirty = false;
  }
}

void exit() {
  state = MENU;
  clearField();
}

}  // namespace talps

const App talps::app = {talps::enter, talps::loop, talps::press, talps::exit};
