// Pong for two players, one at each end of the grid. Each has a 3-LED
// paddle on their edge: top player red on row 0, bottom player blue on
// row 7. Tap your two rows nearest the edge (0-1 or 6-7) where you want
// the paddle; it slides there. The ball speeds up with every hit, and the
// edge of the paddle sends it off at an angle. First to 5 points wins.
//
// After each point both scores show as dots for a moment (row 2 for the
// top player, row 5 for the bottom one), then the ball is served towards
// whoever lost the point. Tap anywhere to start a match.

#include "Apps.h"
#include "BhorealHw.h"

namespace pong {

const uint8_t COLS = 8;
const uint8_t PADDLE = 3;
const uint8_t WIN_POINTS = 5;
const uint16_t START_STEP_MS = 320;
const uint16_t MIN_STEP_MS = 110;
const uint16_t SPEEDUP_MS = 15;
const uint16_t SERVE_DELAY_MS = 700;
const uint16_t PADDLE_MS = 70;   // per cell while a paddle slides
const uint16_t POINT_MS = 1500;
const uint16_t MATCH_OVER_MS = 4000;

enum Player : uint8_t { TOP, BOTTOM };
const uint8_t paddleRow[2] = {0, 7};
const uint8_t color[2][3] = {{200, 0, 0}, {0, 60, 200}};
const uint8_t BALL[3] = {150, 150, 150};

enum State : uint8_t { IDLE, PLAYING, POINT, MATCH_OVER };

State state = IDLE;
uint8_t score[2];
uint8_t pad[2], target[2];  // leftmost paddle column, and where it is sliding to
int8_t ballRow, ballCol, dRow, dCol;
uint16_t stepMs;
uint32_t nextStep, nextPaddle, until;
Player lastWinner;
bool dirty = true;

void serve(Player towards) {
  ballRow = towards == TOP ? 4 : 3;
  ballCol = random(2, 6);
  dRow = towards == TOP ? -1 : 1;
  dCol = random(2) ? 1 : -1;
  stepMs = START_STEP_MS;
  state = PLAYING;
  nextStep = millis() + SERVE_DELAY_MS;
  dirty = true;
}

void startMatch() {
  randomSeed(micros() ^ analogRead(A1));
  score[TOP] = score[BOTTOM] = 0;
  serve(random(2) ? TOP : BOTTOM);
}

void award(Player p) {
  score[p]++;
  lastWinner = p;
  state = score[p] >= WIN_POINTS ? MATCH_OVER : POINT;
  until = millis() + (state == MATCH_OVER ? MATCH_OVER_MS : POINT_MS);
}

void step() {
  int8_t r = ballRow + dRow;
  int8_t c = ballCol + dCol;
  if (c < 0 || c >= COLS) {  // bounce off the side walls
    dCol = -dCol;
    c = ballCol + dCol;
  }
  if (r != paddleRow[TOP] && r != paddleRow[BOTTOM]) {
    ballRow = r;
    ballCol = c;
    return;
  }

  Player p = r == paddleRow[TOP] ? TOP : BOTTOM;
  // Be kind: the paddle counts if it is under where the ball is heading
  // or where it is now.
  int8_t off = c - pad[p];
  if (off < 0 || off >= PADDLE) off = ballCol - pad[p];
  if (off < 0 || off >= PADDLE) {
    ballRow = r;
    ballCol = c;
    award(p == TOP ? BOTTOM : TOP);
    return;
  }
  dRow = -dRow;
  if (off == 0) dCol = -1;
  else if (off == PADDLE - 1) dCol = 1;
  stepMs = stepMs > MIN_STEP_MS + SPEEDUP_MS ? stepMs - SPEEDUP_MS : MIN_STEP_MS;
}

void press(uint8_t row, uint8_t col) {
  dirty = true;
  if (state == IDLE) {
    startMatch();
    return;
  }
  int8_t left = (int8_t)col - PADDLE / 2;
  left = constrain(left, 0, COLS - PADDLE);
  if (row <= 1) target[TOP] = left;
  else if (row >= 6) target[BOTTOM] = left;
}

void update() {
  uint32_t now = millis();
  if ((int32_t)(now - nextPaddle) >= 0) {
    nextPaddle = now + PADDLE_MS;
    for (uint8_t p = 0; p < 2; p++) {
      if (pad[p] == target[p]) continue;
      pad[p] += pad[p] < target[p] ? 1 : -1;
      dirty = true;
    }
  }

  switch (state) {
    case PLAYING:
      if ((int32_t)(now - nextStep) < 0) return;
      nextStep = now + stepMs;
      step();
      dirty = true;
      break;
    case POINT:
      if ((int32_t)(now - until) >= 0) serve(lastWinner == TOP ? BOTTOM : TOP);
      break;
    case MATCH_OVER:
      dirty = true;  // flashing
      if ((int32_t)(now - until) >= 0) state = IDLE;
      break;
    default: break;
  }
}

void render() {
  bhoreal::clear();
  if (state == MATCH_OVER) {
    // The winner's half flashes.
    if ((millis() / 250) % 2 == 0) {
      const uint8_t* w = color[lastWinner];
      uint8_t first = lastWinner == TOP ? 0 : 4;
      for (uint8_t r = first; r < first + 4; r++)
        for (uint8_t k = 0; k < COLS; k++) bhoreal::setPixel(r, k, w[0], w[1], w[2]);
    }
    bhoreal::show();
    return;
  }

  for (uint8_t p = 0; p < 2; p++)
    for (uint8_t i = 0; i < PADDLE; i++)
      bhoreal::setPixel(paddleRow[p], pad[p] + i, color[p][0], color[p][1], color[p][2]);

  if (state == POINT) {
    // Scores, each counted from that player's own left.
    for (uint8_t i = 0; i < score[TOP]; i++)
      bhoreal::setPixel(2, 7 - i, color[TOP][0], color[TOP][1], color[TOP][2]);
    for (uint8_t i = 0; i < score[BOTTOM]; i++)
      bhoreal::setPixel(5, i, color[BOTTOM][0], color[BOTTOM][1], color[BOTTOM][2]);
  }
  if (state == IDLE) bhoreal::setPixel(3, 3, BALL[0], BALL[1], BALL[2]);
  else bhoreal::setPixel(ballRow, ballCol, BALL[0], BALL[1], BALL[2]);
  bhoreal::show();
}

void enter() {
  state = IDLE;
  pad[TOP] = pad[BOTTOM] = target[TOP] = target[BOTTOM] = (COLS - PADDLE) / 2;
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

}  // namespace pong

const App pong::app = {pong::enter, pong::loop, pong::press, pong::exit};
