// Connect 4 for two players: red and yellow take turns dropping discs;
// the first to line up four (across, down or diagonally) wins.
//
// Grid layout:
//   row 0      col 6 = new game (no stop button: a stray press there would
//              wipe a game in progress)
//   row 1      whose turn it is: lit in that player's colour
//   rows 2-7   the board, 8 columns x 6 rows. Tap any cell of a column to
//              drop a disc there; it falls to the lowest free cell.
//
// The four winning discs flash; a full board with no winner is a draw and
// the whole board blinks.

#include "Apps.h"
#include "BhorealHw.h"

namespace connect4 {

const uint8_t CONTROL_ROW = 0;
const uint8_t NEW_COL = 6;
const uint8_t TURN_ROW = 1;
const uint8_t FIRST_ROW = 2;
const uint8_t ROWS = 6;
const uint8_t COLS = 8;
const uint16_t FALL_MS = 60;   // per row while a disc drops

enum Cell : uint8_t { EMPTY, RED, YELLOW };
const uint8_t color[3][3] = {{0, 0, 0}, {200, 0, 0}, {190, 150, 0}};
// The empty board shows a faint blue so the holes are visible.
const uint8_t BOARD[3] = {0, 0, 35};

enum State : uint8_t { PLAYING, DROPPING, WON, DRAW };

State state = PLAYING;
uint8_t board[ROWS][COLS];
Cell player = RED;
uint8_t dropCol, dropRow, dropTarget;  // disc falling: column, current and final row
uint32_t nextFall;
uint8_t winRow[4], winCol[4];
bool dirty = true;

void newGame() {
  memset(board, EMPTY, sizeof(board));
  player = RED;
  state = PLAYING;
  dirty = true;
}

// Look for four in a row through (r, c); remember the cells if found.
bool fourFrom(uint8_t r, uint8_t c) {
  static const int8_t dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  uint8_t who = board[r][c];
  for (const auto& d : dirs) {
    // Walk back to the start of the run, then count forward.
    int8_t sr = r, sc = c;
    while (sr - d[0] >= 0 && sr - d[0] < ROWS && sc - d[1] >= 0 && sc - d[1] < COLS &&
           board[sr - d[0]][sc - d[1]] == who) {
      sr -= d[0];
      sc -= d[1];
    }
    uint8_t n = 0;
    for (int8_t rr = sr, cc = sc; rr >= 0 && rr < ROWS && cc >= 0 && cc < COLS &&
                                  board[rr][cc] == who; rr += d[0], cc += d[1]) {
      if (n < 4) {
        winRow[n] = rr;
        winCol[n] = cc;
      }
      n++;
    }
    if (n >= 4) return true;
  }
  return false;
}

bool boardFull() {
  for (uint8_t c = 0; c < COLS; c++)
    if (board[0][c] == EMPTY) return false;
  return true;
}

void press(uint8_t row, uint8_t col) {
  dirty = true;
  if (row == CONTROL_ROW) {
    if (col == NEW_COL) newGame();
    return;
  }
  if (state != PLAYING || row < FIRST_ROW) return;

  // Lowest free cell in the column.
  int8_t target = -1;
  for (int8_t r = ROWS - 1; r >= 0; r--) {
    if (board[r][col] == EMPTY) {
      target = r;
      break;
    }
  }
  if (target < 0) return;  // column full

  state = DROPPING;
  dropCol = col;
  dropRow = 0;
  dropTarget = target;
  nextFall = millis() + FALL_MS;
}

void update() {
  if (state != DROPPING || (int32_t)(millis() - nextFall) < 0) return;
  dirty = true;
  if (dropRow < dropTarget) {
    dropRow++;
    nextFall += FALL_MS;
    return;
  }
  board[dropTarget][dropCol] = player;
  if (fourFrom(dropTarget, dropCol)) state = WON;
  else if (boardFull()) state = DRAW;
  else {
    player = player == RED ? YELLOW : RED;
    state = PLAYING;
  }
}

bool isWinCell(uint8_t r, uint8_t c) {
  for (uint8_t i = 0; i < 4; i++)
    if (winRow[i] == r && winCol[i] == c) return true;
  return false;
}

void render() {
  bhoreal::clear();
  bool over = state == WON || state == DRAW;
  // Nothing static goes below ~30: these LEDs flicker below that.
  bhoreal::setPixel(CONTROL_ROW, NEW_COL, 0, over ? 120 : 40, 0);

  // Whose turn (or who won).
  const uint8_t* p = color[player];
  for (uint8_t c = 0; c < COLS; c++) bhoreal::setPixel(TURN_ROW, c, p[0] / 2, p[1] / 2, p[2] / 2);

  bool blinkOff = over && (millis() / 250) % 2;
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      uint8_t who = board[r][c];
      const uint8_t* col = who == EMPTY ? BOARD : color[who];
      if (blinkOff && (state == DRAW || (who != EMPTY && isWinCell(r, c)))) col = BOARD;
      bhoreal::setPixel(FIRST_ROW + r, c, col[0], col[1], col[2]);
    }
  }
  if (state == DROPPING) bhoreal::setPixel(FIRST_ROW + dropRow, dropCol, p[0], p[1], p[2]);
  bhoreal::show();
}

void enter() {
  newGame();
}

void loop() {
  update();
  if (state == WON || state == DRAW) dirty = true;  // flashing
  if (dirty) {
    render();
    dirty = false;
  }
}

void exit() {}

}  // namespace connect4

const App connect4::app = {connect4::enter, connect4::loop, connect4::press, connect4::exit};
