// Generative bouncing-balls sequencer.
//
// Grid layout:
//   row 0      col 0 = tempo down, col 1 = tempo up, col 3 = clear all,
//              col 6 = play, col 7 = stop
//   rows 1-7   the field. Press an empty cell to drop a ball moving up;
//              tap a ball to turn it: up -> right -> down -> left -> gone.
//              Hold a ball for 2 s to delete it (it turns red while held).
//
// On every step each ball moves one cell. A ball that reaches a wall
// bounces back and plays a note of C major pentatonic on MIDI channel 1
// (the web app's melody synth plays it):
//   top / bottom walls     note by column, higher to the right;
//                          the bottom wall is an octave lower
//   right / left walls     note by row, higher towards the top;
//                          the left wall is an octave lower

#include "Apps.h"
#include "BhorealHw.h"
#include "UsbMidi.h"

namespace balls {

const uint8_t CONTROL_ROW = 0;
const uint8_t TEMPO_DOWN_COL = 0;
const uint8_t TEMPO_UP_COL = 1;
const uint8_t CLEAR_COL = 3;
const uint8_t PLAY_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t BPM_MIN = 40;
const uint8_t BPM_MAX = 240;
const uint8_t BPM_STEP = 5;

const uint8_t FIELD_FIRST_ROW = 1;
const uint8_t FIELD_LAST_ROW = 7;
const uint8_t COLS = 8;
const uint8_t MAX_BALLS = 16;
const uint8_t MAX_NOTES = MAX_BALLS;
const uint8_t MIDI_CHANNEL = 0;
const uint8_t VELOCITY = 100;
const uint16_t DELETE_HOLD_MS = 2000;
const uint16_t HOLD_SHOW_MS = 300;  // after this, a held ball turns red

enum Dir : uint8_t { UP, RIGHT, DOWN, LEFT, GONE };
const int8_t dRow[4] = {-1, 0, 1, 0};
const int8_t dCol[4] = {0, 1, 0, -1};
const uint8_t dirColor[4][3] = {{0, 140, 160}, {170, 80, 0}, {0, 40, 180}, {160, 0, 140}};

// C major pentatonic: by column (left to right) and by field row (top down).
const uint8_t colNotes[COLS] = {60, 62, 64, 67, 69, 72, 74, 76};
const uint8_t rowNotes[FIELD_LAST_ROW - FIELD_FIRST_ROW + 1] = {79, 76, 74, 72, 69, 67, 64};

struct Ball {
  uint8_t row, col;
  Dir dir;
  bool bounced;  // hit a wall on the last step: drawn white
};

Ball balls[MAX_BALLS];
uint8_t ballCount = 0;
uint8_t sounding[MAX_NOTES];
uint8_t soundingCount = 0;
uint8_t bpm = 120;
bool playing = false;
uint32_t lastStepUs;
bool dirty = true;

// A ball being pressed: turned on release, deleted if held long enough.
int8_t heldBall = -1;
uint8_t heldRow, heldCol;  // the button being held
uint32_t heldSince;

uint32_t stepIntervalUs() {
  return 30000000UL / bpm;  // eighth notes
}

void releaseNotes() {
  for (uint8_t i = 0; i < soundingCount; i++) midi.send(0x80 | MIDI_CHANNEL, sounding[i], 0);
  soundingCount = 0;
}

void playNote(uint8_t note) {
  for (uint8_t i = 0; i < soundingCount; i++)
    if (sounding[i] == note) return;  // two balls on the same note: play it once
  if (soundingCount == MAX_NOTES) return;
  sounding[soundingCount++] = note;
  midi.send(0x90 | MIDI_CHANNEL, note, VELOCITY);
}

// The note a ball plays when it hits the wall in its direction of travel.
uint8_t wallNote(const Ball& b) {
  uint8_t fieldRow = b.row - FIELD_FIRST_ROW;
  switch (b.dir) {
    case UP: return colNotes[b.col];
    case DOWN: return colNotes[b.col] - 12;
    case RIGHT: return rowNotes[fieldRow];
    default: return rowNotes[fieldRow] - 12;
  }
}

void step() {
  releaseNotes();
  for (uint8_t i = 0; i < ballCount; i++) {
    Ball& b = balls[i];
    int8_t row = b.row + dRow[b.dir];
    int8_t col = b.col + dCol[b.dir];
    bool hitsWall = row < FIELD_FIRST_ROW || row > FIELD_LAST_ROW || col < 0 || col >= COLS;
    b.bounced = hitsWall;
    if (hitsWall) {
      playNote(wallNote(b));
      b.dir = (Dir)((b.dir + 2) % 4);  // reverse; it moves away next step
    } else {
      b.row = row;
      b.col = col;
    }
  }
  midi.flush();
}

void start() {
  if (playing) return;
  playing = true;
  lastStepUs = micros();
  step();
}

void stop() {
  playing = false;
  releaseNotes();
  midi.flush();
  for (uint8_t i = 0; i < ballCount; i++) balls[i].bounced = false;
}

void removeBall(uint8_t i) {
  balls[i] = balls[--ballCount];
}

void turnBall(uint8_t i) {
  Ball& b = balls[i];
  b.dir = (Dir)(b.dir + 1);
  b.bounced = false;
  if (b.dir == GONE) removeBall(i);
}

// Turn the held ball on a short tap, delete it after a long hold.
void checkHeldBall() {
  if (heldBall < 0) return;
  uint32_t held = millis() - heldSince;
  if (!bhoreal::isDown(heldRow, heldCol)) {
    if (held < DELETE_HOLD_MS) turnBall(heldBall);
    heldBall = -1;
    dirty = true;
  } else if (held >= DELETE_HOLD_MS) {
    removeBall(heldBall);
    heldBall = -1;
    dirty = true;
  } else if (held >= HOLD_SHOW_MS) {
    dirty = true;  // show it turning red
  }
}

void press(uint8_t row, uint8_t col) {
  dirty = true;
  if (row == CONTROL_ROW) {
    if (col == TEMPO_DOWN_COL && bpm > BPM_MIN) bpm -= BPM_STEP;
    else if (col == TEMPO_UP_COL && bpm < BPM_MAX) bpm += BPM_STEP;
    else if (col == CLEAR_COL) {
      ballCount = 0;
      heldBall = -1;
    }
    else if (col == PLAY_COL) start();
    else if (col == STOP_COL) stop();
    return;
  }

  if (heldBall >= 0) return;  // one ball at a time
  for (uint8_t i = 0; i < ballCount; i++) {
    if (balls[i].row != row || balls[i].col != col) continue;
    heldBall = i;
    heldRow = row;
    heldCol = col;
    heldSince = millis();
    return;
  }
  if (ballCount < MAX_BALLS) balls[ballCount++] = {row, col, UP, false};
}

void render() {
  bhoreal::clear();

  // Nothing static goes below ~30: these LEDs flicker at very low levels.
  bhoreal::setPixel(CONTROL_ROW, TEMPO_DOWN_COL, 0, 0, 40);
  bhoreal::setPixel(CONTROL_ROW, TEMPO_UP_COL, 40, 14, 0);
  bhoreal::setPixel(CONTROL_ROW, CLEAR_COL, 36, 0, 12);
  bhoreal::setPixel(CONTROL_ROW, PLAY_COL, 0, playing ? 70 : 30, 0);
  bhoreal::setPixel(CONTROL_ROW, STOP_COL, playing ? 30 : 70, 0, 0);

  for (uint8_t i = 0; i < ballCount; i++) {
    const Ball& b = balls[i];
    if (i == heldBall && millis() - heldSince >= HOLD_SHOW_MS) bhoreal::setPixel(b.row, b.col, 200, 0, 0);
    else if (b.bounced) bhoreal::setPixel(b.row, b.col, 200, 200, 200);
    else bhoreal::setPixel(b.row, b.col, dirColor[b.dir][0], dirColor[b.dir][1], dirColor[b.dir][2]);
  }
  bhoreal::show();
}

void enter() {
  heldBall = -1;
  dirty = true;
}

void loop() {
  checkHeldBall();
  uint32_t interval = stepIntervalUs();
  if (playing && micros() - lastStepUs >= interval) {
    lastStepUs += interval;
    step();
    dirty = true;
  }
  if (dirty) {
    render();
    dirty = false;
  }
}

void exit() {
  stop();
}

}  // namespace balls

const App balls::app = {balls::enter, balls::loop, balls::press, balls::exit};
