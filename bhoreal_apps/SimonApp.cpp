// Simon app: the Bhoreal plays a growing sequence of coloured pads and the
// player repeats it.
//
// Grid layout:
//   row 0      col 0-2 = speed 1-3 (green to red bar), col 6 = start,
//              col 7 = stop
//   rows 2-5,  four 2x2 pads in the centre: green (top left), red (top
//   cols 2-5   right), yellow (bottom left), blue (bottom right)
//   ring       the 6x6 outline around the pads (rows/cols 1 and 6) flashes
//              white twice when it is the player's turn
//   row 7      length of the current sequence, as a bar of up to 8 LEDs;
//              every further 8 changes the bar colour
//
// Each pad also sends a MIDI note on channel 1, so the web app can play the
// classic Simon tones. At game over the score (rounds completed) is shown
// in digits.

#include "Apps.h"
#include "BhorealHw.h"
#include "UsbMidi.h"

namespace simon {

const uint8_t CONTROL_ROW = 0;
const uint8_t SPEEDS = 3;
const uint8_t START_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t PROGRESS_ROW = 7;
const uint8_t PAD_FIRST = 2;   // pads span rows and cols 2-5
const uint8_t PAD_LAST = 5;
const uint8_t RING_FIRST = 1;  // white outline on rows and cols 1 and 6
const uint8_t RING_LAST = 6;
const uint8_t PADS = 4;
const int8_t NO_PAD = -1;
const uint8_t MAX_LENGTH = 64;

const uint8_t MIDI_CHANNEL = 0;
const uint8_t padNote[PADS] = {64, 69, 61, 52};  // E4, A4, C#4, E3
const uint8_t ERROR_NOTE = 40;
const uint8_t padBright[PADS][3] = {{0, 220, 0}, {220, 0, 0}, {200, 160, 0}, {0, 60, 230}};
// Idle pads stay at 30+ per lit channel: these LEDs flicker below that.
const uint8_t padDim[PADS][3] = {{0, 35, 0}, {35, 0, 0}, {40, 32, 0}, {0, 0, 45}};
// Player's turn: pads stay at medium brightness so it is clear they can play.
const uint8_t padTurn[PADS][3] = {{0, 90, 0}, {90, 0, 0}, {90, 72, 0}, {0, 30, 110}};
// Two white flashes of the ring announce the player's turn: on, off, on.
const uint16_t TURN_FLASH_ON_MS = 500;
const uint16_t TURN_FLASH_OFF_MS = 300;
const uint16_t TURN_FLASH_TOTAL_MS = 2 * TURN_FLASH_ON_MS + TURN_FLASH_OFF_MS;

struct Speed { uint16_t onMs, gapMs; };
const Speed speeds[SPEEDS] = {{600, 250}, {420, 170}, {280, 110}};

enum State : uint8_t { IDLE, SHOWING, PLAYER_TURN, SUCCESS, FAIL, GAME_OVER };

State state = IDLE;
uint8_t speed = 1;
uint8_t sequence[MAX_LENGTH];
uint8_t length;
uint8_t index;          // position being shown or expected from the player
bool padOn;             // during SHOWING: the current pad is lit
uint32_t nextAt;        // when the current phase ends
int8_t litPad = -1;     // pad lit by the game or the player
uint32_t litUntil;
uint32_t turnFlashStart; // when the player's turn flashes began, 0 = none
bool turnFlashLit;       // ring currently lit by the turn flashes
bool dirty = true;

int8_t padAt(uint8_t row, uint8_t col) {
  if (row < PAD_FIRST || row > PAD_LAST || col < PAD_FIRST || col > PAD_LAST) return NO_PAD;
  return (row >= PAD_FIRST + 2 ? 2 : 0) + (col >= PAD_FIRST + 2 ? 1 : 0);
}

bool onRing(uint8_t row, uint8_t col) {
  bool inside = row >= RING_FIRST && row <= RING_LAST && col >= RING_FIRST && col <= RING_LAST;
  return inside && (row == RING_FIRST || row == RING_LAST || col == RING_FIRST || col == RING_LAST);
}

void noteOn(uint8_t note) {
  midi.send(0x90 | MIDI_CHANNEL, note, 100);
  midi.flush();
}

void noteOff(uint8_t note) {
  midi.send(0x80 | MIDI_CHANNEL, note, 0);
  midi.flush();
}

void lightPad(uint8_t pad, uint16_t ms) {
  if (litPad >= 0) noteOff(padNote[litPad]);
  litPad = pad;
  litUntil = millis() + ms;
  noteOn(padNote[pad]);
  dirty = true;
}

void unlightPad() {
  if (litPad < 0) return;
  noteOff(padNote[litPad]);
  litPad = -1;
  dirty = true;
}

// Classic Simon gets quicker after 5, 9 and 13 steps.
uint16_t paced(uint16_t ms) {
  uint8_t pct = length >= 13 ? 70 : length >= 9 ? 80 : length >= 5 ? 90 : 100;
  return (uint32_t)ms * pct / 100;
}

void showSequence(uint16_t delayMs) {
  state = SHOWING;
  index = 0;
  padOn = false;
  nextAt = millis() + delayMs;
}

void addStep() {
  if (length < MAX_LENGTH) sequence[length++] = random(PADS);
}

void startGame() {
  randomSeed(micros() ^ analogRead(A1));
  length = 0;
  addStep();
  showSequence(700);
  dirty = true;
}

void reset() {
  unlightPad();
  noteOff(ERROR_NOTE);
  turnFlashStart = 0;
  turnFlashLit = false;
  state = IDLE;
  dirty = true;
}

void press(uint8_t row, uint8_t col) {
  if (row == CONTROL_ROW) {
    bool busy = state == SHOWING || state == PLAYER_TURN || state == SUCCESS || state == FAIL;
    if (col < SPEEDS && !busy) speed = col + 1;
    else if (col == START_COL && !busy) startGame();
    else if (col == STOP_COL) reset();
    dirty = true;
    return;
  }
  int8_t pad = padAt(row, col);
  if (state != PLAYER_TURN || pad == NO_PAD) return;

  if (pad == sequence[index]) {
    lightPad(pad, 250);
    if (++index == length) {
      state = SUCCESS;
      nextAt = millis() + 800;
    }
  } else {
    unlightPad();
    noteOn(ERROR_NOTE);
    state = FAIL;
    nextAt = millis() + 1200;
  }
  dirty = true;
}

void update() {
  uint32_t now = millis();
  if (litPad >= 0 && state != SHOWING && (int32_t)(now - litUntil) >= 0) unlightPad();
  if (turnFlashStart) {
    uint32_t t = now - turnFlashStart;
    bool lit = t < TURN_FLASH_ON_MS ||
               (t >= TURN_FLASH_ON_MS + TURN_FLASH_OFF_MS && t < TURN_FLASH_TOTAL_MS);
    if (t >= TURN_FLASH_TOTAL_MS) turnFlashStart = 0;
    if (lit != turnFlashLit) {
      turnFlashLit = lit;
      dirty = true;
    }
  }

  if ((int32_t)(now - nextAt) < 0) return;
  const Speed& s = speeds[speed - 1];

  switch (state) {
    case SHOWING:
      if (padOn) {
        unlightPad();
        padOn = false;
        if (++index == length) {
          state = PLAYER_TURN;
          index = 0;
          turnFlashStart = now | 1;
        } else {
          nextAt = now + paced(s.gapMs);
        }
      } else {
        lightPad(sequence[index], paced(s.onMs));
        padOn = true;
        nextAt = now + paced(s.onMs);
      }
      break;
    case SUCCESS:
      addStep();
      showSequence(0);
      break;
    case FAIL:
      noteOff(ERROR_NOTE);
      state = GAME_OVER;
      dirty = true;
      break;
    default:
      break;
  }
}

void render() {
  bhoreal::clear();

  static const uint8_t speedColor[SPEEDS][3] = {{0, 120, 0}, {120, 110, 0}, {150, 0, 0}};
  for (uint8_t i = 0; i < speed; i++)
    bhoreal::setPixel(CONTROL_ROW, i, speedColor[i][0], speedColor[i][1], speedColor[i][2]);
  bool idle = state == IDLE || state == GAME_OVER;
  bhoreal::setPixel(CONTROL_ROW, START_COL, 0, idle ? 80 : 30, 0);
  bhoreal::setPixel(CONTROL_ROW, STOP_COL, idle ? 30 : 80, 0, 0);

  if (length && state != IDLE) {
    static const uint8_t tierColor[4][3] = {{70, 70, 70}, {90, 80, 0}, {110, 40, 0}, {100, 0, 80}};
    uint8_t tier = min((length - 1) / 8, 3);
    uint8_t count = (length - 1) % 8 + 1;
    for (uint8_t i = 0; i < count; i++)
      bhoreal::setPixel(PROGRESS_ROW, i, tierColor[tier][0], tierColor[tier][1], tierColor[tier][2]);
  }

  if (state == GAME_OVER) {
    drawNumber(1, length - 1, 70, 60, 0);
  } else {
    static const uint8_t failColor[3] = {200, 0, 0};
    bool turn = state == PLAYER_TURN;
    for (uint8_t row = PAD_FIRST; row <= PAD_LAST; row++) {
      for (uint8_t col = PAD_FIRST; col <= PAD_LAST; col++) {
        int8_t pad = padAt(row, col);
        const uint8_t* c = (state == FAIL) ? failColor
                         : (pad == litPad) ? padBright[pad]
                         : turn ? padTurn[pad] : padDim[pad];
        bhoreal::setPixel(row, col, c[0], c[1], c[2]);
      }
    }
    if (turn && turnFlashLit) {
      for (uint8_t row = RING_FIRST; row <= RING_LAST; row++)
        for (uint8_t col = RING_FIRST; col <= RING_LAST; col++)
          if (onRing(row, col)) bhoreal::setPixel(row, col, 160, 160, 160);
    }
  }
  bhoreal::show();
}

void enter() {
  reset();
}

void loop() {
  update();
  if (dirty) {
    render();
    dirty = false;
  }
}

void exit() {
  reset();
}

}  // namespace simon

const App simon::app = {simon::enter, simon::loop, simon::press, simon::exit};
