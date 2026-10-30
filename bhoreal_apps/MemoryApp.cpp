// Memory app ("parelles"): find the pairs of hidden colours.
//
// Grid layout:
//   row 0      col 0-4 = level 1-5 as a green-to-red bar, col 6 = start,
//              col 7 = stop
//   board      level 1: 2x4 (4 pairs), 2: 4x4 (8), 3: 4x6 (12),
//              4: 6x6 (18), 5: 6x8 (24), centred below the controls
//
// Only eight colours are easy to tell apart on these LEDs, so from level 3
// on a colour appears more than twice and any two cells of the same colour
// make a pair.
//
// Press a hidden cell to show its colour, then another. A matching pair
// stays lit; otherwise both hide again after a moment (or as soon as a third
// cell is pressed). When every pair is found the board flashes and the
// number of attempts is shown in digits: fewer is better.

#include "Apps.h"
#include "BhorealHw.h"

namespace memory {

const uint8_t CONTROL_ROW = 0;
const uint8_t SIZES = 5;
const uint8_t START_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t MAX_CELLS = 48;
const uint8_t COLORS = 8;
const uint16_t MISMATCH_MS = 900;
const uint16_t WIN_FLASH_MS = 1800;

struct Size { uint8_t firstRow, rows, firstCol, cols; };
const Size sizes[SIZES] = {
  {3, 2, 2, 4},  // 2x4
  {2, 4, 2, 4},  // 4x4
  {2, 4, 1, 6},  // 4x6
  {1, 6, 1, 6},  // 6x6
  {1, 6, 0, 8},  // 6x8
};

// Eight colours that stay easy to tell apart on these LEDs.
const uint8_t colors[COLORS][3] = {
  {200, 0, 0}, {0, 200, 0}, {0, 0, 220}, {180, 150, 0},
  {180, 0, 180}, {0, 160, 180}, {220, 70, 0}, {140, 140, 140},
};
const uint8_t HIDDEN[3] = {30, 30, 30};

enum State : uint8_t { IDLE, PLAYING, WON, GAME_OVER };

State state = IDLE;
uint8_t size = 1;
uint8_t cellColor[MAX_CELLS];  // colour index of each board cell
bool matched[MAX_CELLS];
int8_t first = -1, second = -1;  // cells turned over this attempt
uint32_t hideAt;                 // when a mismatched pair hides again
uint32_t wonAt;
uint8_t attempts;
uint8_t pairsLeft;
bool dirty = true;

uint8_t cellCount() { return sizes[size - 1].rows * sizes[size - 1].cols; }

int8_t cellAt(uint8_t row, uint8_t col) {
  const Size& s = sizes[size - 1];
  if (row < s.firstRow || row >= s.firstRow + s.rows) return -1;
  if (col < s.firstCol || col >= s.firstCol + s.cols) return -1;
  return (row - s.firstRow) * s.cols + (col - s.firstCol);
}

void startGame() {
  randomSeed(micros() ^ analogRead(A1));
  uint8_t n = cellCount();
  for (uint8_t i = 0; i < n; i++) {
    cellColor[i] = (i / 2) % COLORS;
    matched[i] = false;
  }
  for (uint8_t i = n - 1; i > 0; i--) {  // Fisher-Yates shuffle
    uint8_t j = random(i + 1);
    uint8_t t = cellColor[i];
    cellColor[i] = cellColor[j];
    cellColor[j] = t;
  }
  first = second = -1;
  attempts = 0;
  pairsLeft = n / 2;
  state = PLAYING;
}

void hidePair() {
  first = second = -1;
  dirty = true;
}

void press(uint8_t row, uint8_t col) {
  dirty = true;
  if (row == CONTROL_ROW) {
    bool busy = state == PLAYING || state == WON;
    if (col < SIZES && !busy) size = col + 1;
    else if (col == START_COL && !busy) startGame();
    else if (col == STOP_COL) state = IDLE;
    return;
  }
  if (state != PLAYING) return;

  int8_t cell = cellAt(row, col);
  if (cell < 0 || matched[cell] || cell == first || cell == second) return;

  if (second >= 0) hidePair();  // a third press hides the mismatched pair at once
  if (first < 0) {
    first = cell;
    return;
  }

  second = cell;
  attempts++;
  if (cellColor[first] == cellColor[second]) {
    matched[first] = matched[second] = true;
    first = second = -1;
    if (--pairsLeft == 0) {
      state = WON;
      wonAt = millis();
    }
  } else {
    hideAt = millis() + MISMATCH_MS;
  }
}

void update() {
  uint32_t now = millis();
  if (state == PLAYING && second >= 0 && (int32_t)(now - hideAt) >= 0) hidePair();
  if (state == WON) {
    dirty = true;  // the win flash animates
    if (now - wonAt >= WIN_FLASH_MS) state = GAME_OVER;
  }
}

void render() {
  bhoreal::clear();

  static const uint8_t sizeColor[SIZES][3] = {
    {0, 120, 0}, {70, 120, 0}, {120, 110, 0}, {140, 50, 0}, {150, 0, 0},
  };
  for (uint8_t i = 0; i < size; i++)
    bhoreal::setPixel(CONTROL_ROW, i, sizeColor[i][0], sizeColor[i][1], sizeColor[i][2]);
  bool idle = state == IDLE || state == GAME_OVER;
  bhoreal::setPixel(CONTROL_ROW, START_COL, 0, idle ? 80 : 30, 0);
  bhoreal::setPixel(CONTROL_ROW, STOP_COL, idle ? 30 : 80, 0, 0);

  if (state == GAME_OVER) {
    drawNumber(1, attempts, 70, 60, 0);
    bhoreal::show();
    return;
  }

  // Win: the whole board blinks in its colours.
  bool winBlinkOff = state == WON && ((millis() - wonAt) / 300) % 2;
  const Size& s = sizes[size - 1];
  for (uint8_t r = 0; r < s.rows; r++) {
    for (uint8_t c = 0; c < s.cols; c++) {
      uint8_t cell = r * s.cols + c;
      const uint8_t* color = HIDDEN;
      if (state != IDLE && (matched[cell] || cell == first || cell == second))
        color = colors[cellColor[cell]];
      if (winBlinkOff) continue;
      bhoreal::setPixel(s.firstRow + r, s.firstCol + c, color[0], color[1], color[2]);
    }
  }
  bhoreal::show();
}

void enter() {
  state = IDLE;
  first = second = -1;
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

}  // namespace memory

const App memory::app = {memory::enter, memory::loop, memory::press, memory::exit};
